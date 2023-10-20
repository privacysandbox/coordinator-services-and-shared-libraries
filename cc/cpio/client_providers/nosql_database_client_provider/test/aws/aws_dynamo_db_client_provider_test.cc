// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cpio/client_providers/nosql_database_client_provider/src/aws/aws_dynamo_db_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/dynamodb/DynamoDBErrors.h>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/nosql_database_client_provider/src/common/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::InitAPI;
using Aws::Map;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::String;
using Aws::Client::AsyncCallerContext;
using Aws::Client::AWSError;
using Aws::Client::ClientConfiguration;
using Aws::DynamoDB::DynamoDBClient;
using Aws::DynamoDB::DynamoDBErrors;
using Aws::DynamoDB::GetItemResponseReceivedHandler;
using Aws::DynamoDB::QueryResponseReceivedHandler;
using Aws::DynamoDB::UpdateItemResponseReceivedHandler;
using Aws::DynamoDB::Model::AttributeValue;
using Aws::DynamoDB::Model::GetItemRequest;
using Aws::DynamoDB::Model::QueryOutcome;
using Aws::DynamoDB::Model::QueryRequest;
using Aws::DynamoDB::Model::QueryResult;
using Aws::DynamoDB::Model::UpdateItemOutcome;
using Aws::DynamoDB::Model::UpdateItemRequest;
using Aws::DynamoDB::Model::UpdateItemResult;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse;
using google::cmrt::sdk::nosql_database_service::v1::ItemAttribute;
using google::cmrt::sdk::nosql_database_service::v1::ItemKey;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using std::atomic;
using std::atomic_bool;
using std::get;
using std::isxdigit;
using std::make_pair;
using std::make_shared;
using std::move;
using std::mt19937;
using std::random_device;
using std::shared_ptr;
using std::string;
using std::uniform_int_distribution;
using std::vector;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::NiceMock;
using testing::Pair;
using testing::Return;
using testing::UnorderedElementsAre;

namespace {

constexpr char kTableName[] = "TestTable";

ItemAttribute MakeStringAttribute(const string& name, const string& value) {
  ItemAttribute attribute;
  attribute.set_name(name);
  attribute.set_value_string(value);
  return attribute;
}

ItemAttribute MakeIntAttribute(const string& name, int value) {
  ItemAttribute attribute;
  attribute.set_name(name);
  attribute.set_value_int(value);
  return attribute;
}

ItemAttribute MakeDoubleAttribute(const string& name, double value) {
  ItemAttribute attribute;
  attribute.set_name(name);
  attribute.set_value_double(value);
  return attribute;
}

}  // namespace

namespace google::scp::cpio::client_providers::test {

class MockDynamoDBFactory : public DynamoDBFactory {
 public:
  MOCK_METHOD(ExecutionResultOr<shared_ptr<DynamoDBClient>>, CreateClient,
              (const ClientConfiguration&), (noexcept, override));
};

class MockDynamoDBClient : public Aws::DynamoDB::DynamoDBClient {
 public:
  MOCK_METHOD(void, QueryAsync,
              (const Aws::DynamoDB::Model::QueryRequest&,
               const Aws::DynamoDB::QueryResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));

  MOCK_METHOD(void, UpdateItemAsync,
              (const Aws::DynamoDB::Model::UpdateItemRequest&,
               const Aws::DynamoDB::UpdateItemResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));
};

class AwsDynamoDBClientProviderTest : public testing::Test {
 protected:
  static void SetUpTestSuite() { InitAPI(SDKOptions()); }

  static void TearDownTestSuite() { ShutdownAPI(SDKOptions()); }

  AwsDynamoDBClientProviderTest()
      : instance_client_(make_shared<MockInstanceClientProvider>()),
        dynamo_db_(make_shared<NiceMock<MockDynamoDBClient>>()),
        dynamo_db_factory_(make_shared<NiceMock<MockDynamoDBFactory>>()),
        client_provider_(instance_client_, make_shared<MockAsyncExecutor>(),
                         make_shared<MockAsyncExecutor>(), dynamo_db_factory_) {
    ON_CALL(*dynamo_db_factory_, CreateClient)
        .WillByDefault(Return(dynamo_db_));

    get_database_item_context_.request = make_shared<GetDatabaseItemRequest>();

    get_database_item_context_.callback = [this](auto) {
      finish_called_ = true;
    };

    upsert_database_item_context_.request =
        make_shared<UpsertDatabaseItemRequest>();

    upsert_database_item_context_.callback = [this](auto) {
      finish_called_ = true;
    };

    EXPECT_SUCCESS(client_provider_.Init());
    EXPECT_SUCCESS(client_provider_.Run());
  }

  shared_ptr<MockInstanceClientProvider> instance_client_;
  shared_ptr<MockDynamoDBClient> dynamo_db_;
  shared_ptr<MockDynamoDBFactory> dynamo_db_factory_;
  AwsDynamoDBClientProvider client_provider_;

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context_;

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context_;
  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  atomic_bool finish_called_{false};
};

MATCHER_P2(IsIntAttribute, name, value, "") {
  if (arg.value_case() != ItemAttribute::kValueInt) {
    *result_listener << "Expected arg to have value_int: " << value
                     << " but has:\n"
                     << arg.DebugString();
    return false;
  }
  return ExplainMatchResult(Eq(value), arg.value_int(), result_listener) &&
         ExplainMatchResult(Eq(name), arg.name(), result_listener);
}

MATCHER_P2(IsDoubleAttribute, name, value, "") {
  if (arg.value_case() != ItemAttribute::kValueDouble) {
    *result_listener << "Expected arg to have value_double: " << value
                     << " but has:\n"
                     << arg.DebugString();
    return false;
  }
  return ExplainMatchResult(Eq(value), arg.value_double(), result_listener) &&
         ExplainMatchResult(Eq(name), arg.name(), result_listener);
}

MATCHER_P2(IsStringAttribute, name, value, "") {
  if (arg.value_case() != ItemAttribute::kValueString) {
    *result_listener << "Expected arg to have value_string: " << value
                     << " but has:\n"
                     << arg.DebugString();
    return false;
  }
  return ExplainMatchResult(Eq(value), arg.value_string(), result_listener) &&
         ExplainMatchResult(Eq(name), arg.name(), result_listener);
}

MATCHER_P(HasNumber, num, "") {
  return ExplainMatchResult(Eq(num), arg.GetN(), result_listener);
}

TEST_F(AwsDynamoDBClientProviderTest, RunWithCreateClientConfigurationFailed) {
  auto failure_result = FailureExecutionResult(SC_UNKNOWN);
  instance_client_->get_instance_resource_name_mock = failure_result;

  EXPECT_SUCCESS(client_provider_.Init());
  EXPECT_THAT(client_provider_.Run(), ResultIs(failure_result));
}

TEST_F(AwsDynamoDBClientProviderTest,
       GetItemWithPartitionAndSortKeyWithoutAttributes) {
  auto& key = *get_database_item_context_.request->mutable_key();
  key.set_table_name(kTableName);
  key.mutable_partition_key()->set_name("Col1");
  key.mutable_partition_key()->set_value_int(3);
  key.mutable_sort_key()->set_name("Col2");
  key.mutable_sort_key()->set_value_int(2);

  EXPECT_CALL(*dynamo_db_, QueryAsync)
      .WillOnce([this](const auto& query_request, const auto& callback,
                       const auto&) {
        EXPECT_EQ(query_request.GetTableName(),
                  get_database_item_context_.request->key().table_name());
        EXPECT_EQ(query_request.GetKeyConditionExpression(),
                  "Col1= :partition_key and Col2= :sort_key");
        EXPECT_THAT(query_request.GetExpressionAttributeValues(),
                    UnorderedElementsAre(Pair(":partition_key", HasNumber("3")),
                                         Pair(":sort_key", HasNumber("2"))));
        EXPECT_TRUE(query_request.GetFilterExpression().empty());
        QueryResult query_result;
        Map<String, AttributeValue> item;

        item.emplace(String("Col1"), AttributeValue().SetN("3"));
        item.emplace(String("Col2"), AttributeValue().SetN("2"));
        item.emplace(String("attr1"), AttributeValue().SetS("hello world"));
        item.emplace(String("attr2"), AttributeValue().SetN("2"));
        item.emplace(String("attr3"), AttributeValue().SetN("3"));

        query_result.AddItems(item);

        QueryOutcome outcome(query_result);
        callback(nullptr /*dynamo_client*/, query_request, outcome,
                 nullptr /*caller_context*/);
      });

  get_database_item_context_.callback =
      [this](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
                 context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->item().key().table_name(), kTableName);
        EXPECT_THAT(context.response->item().key().partition_key(),
                    IsIntAttribute("Col1", 3));
        EXPECT_THAT(context.response->item().key().sort_key(),
                    IsIntAttribute("Col2", 2));
        EXPECT_THAT(
            context.response->item().attributes(),
            UnorderedElementsAre(IsStringAttribute("attr1", "hello world"),
                                 IsIntAttribute("attr2", 2),
                                 IsIntAttribute("attr3", 3)));
        finish_called_ = true;
      };

  EXPECT_THAT(client_provider_.GetDatabaseItem(get_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

MATCHER_P(HasString, str, "") {
  return ExplainMatchResult(Eq(str), arg.GetS(), result_listener);
}

TEST_F(AwsDynamoDBClientProviderTest,
       GetItemWithPartitionAndSortKeyWithAttributes) {
  auto& key = *get_database_item_context_.request->mutable_key();
  key.set_table_name(kTableName);
  key.mutable_partition_key()->set_name("Col1");
  key.mutable_partition_key()->set_value_int(3);
  key.mutable_sort_key()->set_name("Col2");
  key.mutable_sort_key()->set_value_int(2);

  *get_database_item_context_.request->add_required_attributes() =
      MakeStringAttribute("Attr1", "1234");
  *get_database_item_context_.request->add_required_attributes() =
      MakeIntAttribute("Attr2", 2);
  *get_database_item_context_.request->add_required_attributes() =
      MakeDoubleAttribute("Attr3", 4.5);

  EXPECT_CALL(*dynamo_db_, QueryAsync)
      .WillOnce([this](const auto& query_request, const auto& callback,
                       const auto&) {
        EXPECT_EQ(query_request.GetTableName(),
                  get_database_item_context_.request->key().table_name());
        EXPECT_EQ(query_request.GetKeyConditionExpression(),
                  "Col1= :partition_key and Col2= :sort_key");
        EXPECT_EQ(query_request.GetFilterExpression(),
                  "Attr1= :attribute_0 and Attr2= :attribute_1 and Attr3= "
                  ":attribute_2");
        EXPECT_THAT(
            query_request.GetExpressionAttributeValues(),
            UnorderedElementsAre(Pair(":partition_key", HasNumber("3")),
                                 Pair(":sort_key", HasNumber("2")),
                                 Pair(":attribute_0", HasString("1234")),
                                 Pair(":attribute_1", HasNumber("2")),
                                 Pair(":attribute_2", HasNumber("4.500000"))));
        QueryResult query_result;
        Map<String, AttributeValue> item;

        item.emplace(String("Col1"), AttributeValue().SetN("3"));
        item.emplace(String("Col2"), AttributeValue().SetN("2"));
        item.emplace(String("attr1"), AttributeValue().SetS("hello world"));
        item.emplace(String("attr2"), AttributeValue().SetN("2"));
        item.emplace(String("attr3"), AttributeValue().SetN("3"));
        item.emplace(String("Attr1"), AttributeValue().SetS("1234"));
        item.emplace(String("Attr2"), AttributeValue().SetN("2"));
        item.emplace(String("Attr3"), AttributeValue().SetN("4.5"));

        query_result.AddItems(item);

        QueryOutcome outcome(query_result);
        callback(nullptr /*dynamo_client*/, query_request, outcome,
                 nullptr /*caller_context*/);
      });

  get_database_item_context_.callback =
      [this](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
                 context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->item().key().table_name(), kTableName);
        EXPECT_THAT(context.response->item().key().partition_key(),
                    IsIntAttribute("Col1", 3));
        EXPECT_THAT(context.response->item().key().sort_key(),
                    IsIntAttribute("Col2", 2));
        EXPECT_THAT(
            context.response->item().attributes(),
            UnorderedElementsAre(
                IsStringAttribute("attr1", "hello world"),
                IsIntAttribute("attr2", 2), IsIntAttribute("attr3", 3),
                IsStringAttribute("Attr1", "1234"), IsIntAttribute("Attr2", 2),
                IsDoubleAttribute("Attr3", 4.5)));
        finish_called_ = true;
      };

  EXPECT_THAT(client_provider_.GetDatabaseItem(get_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsDynamoDBClientProviderTest, GetDatabaseItemFailure) {
  auto& key = *get_database_item_context_.request->mutable_key();
  key.set_table_name(kTableName);
  key.mutable_partition_key()->set_name("Col1");
  key.mutable_partition_key()->set_value_int(3);
  key.mutable_sort_key()->set_name("Col2");
  key.mutable_sort_key()->set_value_int(2);

  EXPECT_CALL(*dynamo_db_, QueryAsync)
      .WillOnce([this](const auto& query_request, const auto& callback,
                       const auto&) {
        EXPECT_EQ(query_request.GetTableName(),
                  get_database_item_context_.request->key().table_name());
        EXPECT_EQ(query_request.GetKeyConditionExpression(),
                  "Col1= :partition_key and Col2= :sort_key");
        EXPECT_THAT(query_request.GetExpressionAttributeValues(),
                    UnorderedElementsAre(Pair(":partition_key", HasNumber("3")),
                                         Pair(":sort_key", HasNumber("2"))));
        EXPECT_TRUE(query_request.GetFilterExpression().empty());

        AWSError<DynamoDBErrors> dynamo_db_error(DynamoDBErrors::BACKUP_IN_USE,
                                                 false);

        QueryOutcome outcome(dynamo_db_error);

        callback(nullptr /*dynamo_client*/, query_request, outcome,
                 nullptr /*caller_context*/);
      });

  get_database_item_context_.callback =
      [this](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
                 context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)));
        finish_called_ = true;
      };

  EXPECT_THAT(client_provider_.GetDatabaseItem(get_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsDynamoDBClientProviderTest, GetDatabaseItemZeroResults) {
  auto& key = *get_database_item_context_.request->mutable_key();
  key.set_table_name(kTableName);
  key.mutable_partition_key()->set_name("Col1");
  key.mutable_partition_key()->set_value_int(3);
  key.mutable_sort_key()->set_name("Col2");
  key.mutable_sort_key()->set_value_int(2);

  EXPECT_CALL(*dynamo_db_, QueryAsync)
      .WillOnce([this](const auto& query_request, const auto& callback,
                       const auto&) {
        EXPECT_EQ(query_request.GetTableName(),
                  get_database_item_context_.request->key().table_name());
        EXPECT_EQ(query_request.GetKeyConditionExpression(),
                  "Col1= :partition_key and Col2= :sort_key");
        EXPECT_THAT(query_request.GetExpressionAttributeValues(),
                    UnorderedElementsAre(Pair(":partition_key", HasNumber("3")),
                                         Pair(":sort_key", HasNumber("2"))));
        EXPECT_TRUE(query_request.GetFilterExpression().empty());

        QueryResult query_result;
        QueryOutcome outcome(query_result);

        callback(nullptr /*dynamo_client*/, query_request, outcome,
                 nullptr /*caller_context*/);
      });

  get_database_item_context_.callback =
      [this](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
                 context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND)));
        finish_called_ = true;
      };

  EXPECT_THAT(client_provider_.GetDatabaseItem(get_database_item_context_),
              IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsDynamoDBClientProviderTest,
       UpsertItemWithPartitionAndSortKeyWithoutRequiredAttributes) {
  auto& key = *upsert_database_item_context_.request->mutable_key();
  key.set_table_name(kTableName);
  key.mutable_partition_key()->set_name("Col1");
  key.mutable_partition_key()->set_value_int(3);
  key.mutable_sort_key()->set_name("Col2");
  key.mutable_sort_key()->set_value_int(2);
  auto& new_attributes =
      *upsert_database_item_context_.request->mutable_new_attributes();
  new_attributes.Add(MakeStringAttribute("Attr1", "1234"));
  new_attributes.Add(MakeIntAttribute("Attr2", 2));
  new_attributes.Add(MakeDoubleAttribute("Attr3", 4.5));

  EXPECT_CALL(*dynamo_db_, UpdateItemAsync)
      .WillOnce([this](const auto& update_item_request, const auto& callback,
                       const auto&) {
        EXPECT_EQ(update_item_request.GetTableName(),
                  upsert_database_item_context_.request->key().table_name());
        EXPECT_THAT(update_item_request.GetKey(),
                    UnorderedElementsAre(Pair("Col1", HasNumber("3")),
                                         Pair("Col2", HasNumber("2"))));
        EXPECT_THAT(update_item_request.GetExpressionAttributeValues(),
                    UnorderedElementsAre(
                        Pair(":new_attribute_0", HasString("1234")),
                        Pair(":new_attribute_1", HasNumber("2")),
                        Pair(":new_attribute_2", HasNumber("4.500000"))));
        EXPECT_EQ(update_item_request.GetUpdateExpression(),
                  "SET Attr1= :new_attribute_0 , Attr2= :new_attribute_1 , "
                  "Attr3= :new_attribute_2");

        UpdateItemResult update_item_result;
        UpdateItemOutcome outcome(update_item_result);

        callback(nullptr /*dynamo_client*/, update_item_request, outcome,
                 nullptr /*caller_context*/);
      });

  upsert_database_item_context_.callback =
      [this](AsyncContext<UpsertDatabaseItemRequest,
                          UpsertDatabaseItemResponse>& context) {
        EXPECT_SUCCESS(context.result);
        finish_called_ = true;
      };

  EXPECT_THAT(
      client_provider_.UpsertDatabaseItem(upsert_database_item_context_),
      IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsDynamoDBClientProviderTest,
       UpsertItemWithPartitionAndSortKeyWithRequiredAttributes) {
  auto& key = *upsert_database_item_context_.request->mutable_key();
  key.set_table_name(kTableName);
  key.mutable_partition_key()->set_name("Col1");
  key.mutable_partition_key()->set_value_int(3);
  key.mutable_sort_key()->set_name("Col2");
  key.mutable_sort_key()->set_value_int(2);

  auto& new_attributes =
      *upsert_database_item_context_.request->mutable_new_attributes();
  new_attributes.Add(MakeStringAttribute("Attr1", "1234"));
  new_attributes.Add(MakeIntAttribute("Attr2", 2));
  new_attributes.Add(MakeDoubleAttribute("Attr3", 4.5));

  auto& required_attributes =
      *upsert_database_item_context_.request->mutable_required_attributes();
  required_attributes.Add(MakeStringAttribute("Attr1", "4321"));
  required_attributes.Add(MakeIntAttribute("Attr2", 20));
  required_attributes.Add(MakeDoubleAttribute("Attr3", 5.4));

  EXPECT_CALL(*dynamo_db_, UpdateItemAsync)
      .WillOnce([this](const auto& update_item_request, const auto& callback,
                       const auto&) {
        EXPECT_EQ(update_item_request.GetTableName(),
                  upsert_database_item_context_.request->key().table_name());
        EXPECT_THAT(update_item_request.GetKey(),
                    UnorderedElementsAre(Pair("Col1", HasNumber("3")),
                                         Pair("Col2", HasNumber("2"))));
        EXPECT_THAT(update_item_request.GetExpressionAttributeValues(),
                    UnorderedElementsAre(
                        Pair(":new_attribute_0", HasString("1234")),
                        Pair(":new_attribute_1", HasNumber("2")),
                        Pair(":new_attribute_2", HasNumber("4.500000")),
                        Pair(":attribute_0", HasString("4321")),
                        Pair(":attribute_1", HasNumber("20")),
                        Pair(":attribute_2", HasNumber("5.400000"))));
        EXPECT_EQ(update_item_request.GetUpdateExpression(),
                  "SET Attr1= :new_attribute_0 , Attr2= :new_attribute_1 , "
                  "Attr3= :new_attribute_2");
        EXPECT_EQ(update_item_request.GetConditionExpression(),
                  "Attr1= :attribute_0 and Attr2= :attribute_1 and "
                  "Attr3= :attribute_2");

        UpdateItemResult update_item_result;
        UpdateItemOutcome outcome(update_item_result);

        callback(nullptr /*dynamo_client*/, update_item_request, outcome,
                 nullptr /*caller_context*/);
      });

  upsert_database_item_context_.callback =
      [this](AsyncContext<UpsertDatabaseItemRequest,
                          UpsertDatabaseItemResponse>& context) {
        EXPECT_SUCCESS(context.result);
        finish_called_ = true;
      };

  EXPECT_THAT(
      client_provider_.UpsertDatabaseItem(upsert_database_item_context_),
      IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsDynamoDBClientProviderTest, UpsertDatabaseItemCallbackFailure) {
  auto& key = *upsert_database_item_context_.request->mutable_key();
  key.set_table_name(kTableName);
  key.mutable_partition_key()->set_name("Col1");
  key.mutable_partition_key()->set_value_int(3);
  key.mutable_sort_key()->set_name("Col2");
  key.mutable_sort_key()->set_value_int(2);
  auto& new_attributes =
      *upsert_database_item_context_.request->mutable_new_attributes();
  new_attributes.Add(MakeStringAttribute("Attr1", "1234"));
  new_attributes.Add(MakeIntAttribute("Attr2", 2));
  new_attributes.Add(MakeDoubleAttribute("Attr3", 4.5));

  EXPECT_CALL(*dynamo_db_, UpdateItemAsync)
      .WillOnce([this](const auto& update_item_request, const auto& callback,
                       const auto&) {
        EXPECT_EQ(update_item_request.GetTableName(),
                  upsert_database_item_context_.request->key().table_name());
        EXPECT_THAT(update_item_request.GetKey(),
                    UnorderedElementsAre(Pair("Col1", HasNumber("3")),
                                         Pair("Col2", HasNumber("2"))));
        EXPECT_THAT(update_item_request.GetExpressionAttributeValues(),
                    UnorderedElementsAre(
                        Pair(":new_attribute_0", HasString("1234")),
                        Pair(":new_attribute_1", HasNumber("2")),
                        Pair(":new_attribute_2", HasNumber("4.500000"))));
        EXPECT_EQ(update_item_request.GetUpdateExpression(),
                  "SET Attr1= :new_attribute_0 , Attr2= :new_attribute_1 , "
                  "Attr3= :new_attribute_2");

        AWSError<DynamoDBErrors> dynamo_db_error(DynamoDBErrors::BACKUP_IN_USE,
                                                 false);

        UpdateItemOutcome outcome(dynamo_db_error);

        callback(nullptr /*dynamo_client*/, update_item_request, outcome,
                 nullptr /*caller_context*/);
      });

  upsert_database_item_context_.callback =
      [this](AsyncContext<UpsertDatabaseItemRequest,
                          UpsertDatabaseItemResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)));
        finish_called_ = true;
      };

  EXPECT_THAT(
      client_provider_.UpsertDatabaseItem(upsert_database_item_context_),
      IsSuccessful());

  WaitUntil([this]() { return finish_called_.load(); });
}

}  // namespace google::scp::cpio::client_providers::test
