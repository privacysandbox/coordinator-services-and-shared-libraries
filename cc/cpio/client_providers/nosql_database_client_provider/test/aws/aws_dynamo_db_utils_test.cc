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

#include "cpio/client_providers/nosql_database_client_provider/src/aws/aws_dynamo_db_utils.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/interface/async_context.h"
#include "cpio/client_providers/nosql_database_client_provider/src/common/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::Map;
using Aws::String;
using Aws::Vector;
using Aws::DynamoDB::DynamoDBErrors;
using Aws::DynamoDB::Model::AttributeValue;
using Aws::DynamoDB::Model::ValueType;
using Aws::Utils::ByteBuffer;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse;
using google::cmrt::sdk::nosql_database_service::v1::ItemAttribute;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_CONDITIONAL_CHECKED_FAILED;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;
using std::get;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using testing::Eq;
using testing::Optional;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace google::scp::cpio::client_providers {

MATCHER_P(HasValueInt, value, "") {
  if (arg.value_case() != ItemAttribute::kValueInt) {
    *result_listener << "Expected arg to have value_int: " << value
                     << " but has:\n"
                     << arg.DebugString();
    return false;
  }
  return ExplainMatchResult(Eq(value), arg.value_int(), result_listener);
}

MATCHER_P(HasValueDouble, value, "") {
  if (arg.value_case() != ItemAttribute::kValueDouble) {
    *result_listener << "Expected arg to have value_double: " << value
                     << " but has:\n"
                     << arg.DebugString();
    return false;
  }
  return ExplainMatchResult(Eq(value), arg.value_double(), result_listener);
}

TEST(AwsDynamoDBUtilsTests, ConvertDynamoDBTypeToItemAttributeNumber) {
  AttributeValue attribute_value;
  attribute_value.SetN("123");

  EXPECT_THAT(
      AwsDynamoDBUtils::ConvertDynamoDBTypeToItemAttribute(attribute_value),
      IsSuccessfulAndHolds(HasValueInt(123)));

  attribute_value.SetN("123.0");
  EXPECT_THAT(
      AwsDynamoDBUtils::ConvertDynamoDBTypeToItemAttribute(attribute_value),
      IsSuccessfulAndHolds(HasValueDouble(123.0)));
}

MATCHER_P(HasValueString, value, "") {
  if (arg.value_case() != ItemAttribute::kValueString) {
    *result_listener << "Expected arg to have value_string: " << value
                     << " but has:\n"
                     << arg.DebugString();
    return false;
  }
  return ExplainMatchResult(Eq(value), arg.value_string(), result_listener);
}

TEST(AwsDynamoDBUtilsTests, ConvertDynamoDBTypeToItemAttributeString) {
  AttributeValue attribute_value;
  attribute_value.SetS("hello world!");

  EXPECT_THAT(
      AwsDynamoDBUtils::ConvertDynamoDBTypeToItemAttribute(attribute_value),
      IsSuccessfulAndHolds(HasValueString("hello world!")));
}

void SetValueType(AttributeValue& attribute_value, ValueType type) {
  switch (type) {
    case ValueType::BYTEBUFFER:
      attribute_value.SetB(ByteBuffer());
      break;
    case ValueType::STRING_SET:
      attribute_value.SetSS(Vector<String>());
      break;
    case ValueType::NUMBER_SET:
      attribute_value.SetNS(Vector<String>());
      break;
    case ValueType::BYTEBUFFER_SET:
      attribute_value.SetBS(Vector<ByteBuffer>());
      break;
    case ValueType::ATTRIBUTE_MAP:
      attribute_value.SetM(Map<String, const shared_ptr<AttributeValue>>());
      break;
    case ValueType::ATTRIBUTE_LIST:
      attribute_value.SetL(Vector<shared_ptr<AttributeValue>>());
      break;
    case ValueType::NULLVALUE:
      attribute_value.SetNull(false);
      break;
    case ValueType::BOOL:
      attribute_value.SetB(false);
      break;
    default:
      FAIL() << "Invalid type";
  }
}

TEST(AwsDynamoDBUtilsTests, ConvertDynamoDBTypeToItemAttributeInvalidType) {
  vector<ValueType> invalid_value_types(
      {ValueType::BYTEBUFFER, ValueType::STRING_SET, ValueType::NUMBER_SET,
       ValueType::BYTEBUFFER_SET, ValueType::ATTRIBUTE_MAP,
       ValueType::ATTRIBUTE_LIST, ValueType::BOOL, ValueType::NULLVALUE});

  for (auto value_type : invalid_value_types) {
    AttributeValue attribute_value;
    SetValueType(attribute_value, value_type);
    EXPECT_THAT(
        AwsDynamoDBUtils::ConvertDynamoDBTypeToItemAttribute(attribute_value),
        ResultIs(FailureExecutionResult(
            SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE)));
  }
}

TEST(AwsDynamoDBUtilsTests, ConvertItemAttributeToDynamoDBTypeNumber) {
  AttributeValue attribute_value;
  attribute_value.SetN("1");
  ItemAttribute attribute;
  attribute.set_value_int(1);
  EXPECT_THAT(AwsDynamoDBUtils::ConvertItemAttributeToDynamoDBType(attribute),
              IsSuccessfulAndHolds(attribute_value));

  attribute.set_value_double(1.3);
  attribute_value.SetN("1.300000");
  EXPECT_THAT(AwsDynamoDBUtils::ConvertItemAttributeToDynamoDBType(attribute),
              IsSuccessfulAndHolds(attribute_value));

  attribute.set_value_float(1.5);
  attribute_value.SetN("1.500000");
  EXPECT_THAT(AwsDynamoDBUtils::ConvertItemAttributeToDynamoDBType(attribute),
              IsSuccessfulAndHolds(attribute_value));

  // Edge case - when converting to string, the '.' and trailing zeroes may be
  // dropped. Ensure special handling.
  attribute.set_value_double(1);
  attribute_value.SetN("1.000000");
  EXPECT_THAT(AwsDynamoDBUtils::ConvertItemAttributeToDynamoDBType(attribute),
              IsSuccessfulAndHolds(attribute_value));
}

TEST(AwsDynamoDBUtilsTests, ConvertItemAttributeToDynamoDBTypeString) {
  AttributeValue attribute_value;
  ItemAttribute attribute;
  attribute.set_value_string("hello world!");
  attribute_value.SetS("hello world!");
  EXPECT_THAT(AwsDynamoDBUtils::ConvertItemAttributeToDynamoDBType(attribute),
              IsSuccessfulAndHolds(attribute_value));
}

class AwsDynamoDBErrorTest
    : public testing::TestWithParam<pair<DynamoDBErrors, ExecutionResult>> {
 protected:
  DynamoDBErrors GetErrorToConvert() { return get<0>(GetParam()); }

  ExecutionResult GetExpectedResult() { return get<1>(GetParam()); }
};

TEST_P(AwsDynamoDBErrorTest, ConvertDynamoErrorToExecutionResult) {
  EXPECT_THAT(AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
                  GetErrorToConvert()),
              ResultIs(GetExpectedResult()));
}

INSTANTIATE_TEST_SUITE_P(
    Errors, AwsDynamoDBErrorTest,
    testing::Values(
        // Retriable error codes.
        make_pair(
            DynamoDBErrors::INTERNAL_FAILURE,
            RetryExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR)),
        make_pair(
            DynamoDBErrors::SERVICE_UNAVAILABLE,
            RetryExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR)),
        make_pair(
            DynamoDBErrors::THROTTLING,
            RetryExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR)),
        make_pair(
            DynamoDBErrors::SLOW_DOWN,
            RetryExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR)),
        make_pair(
            DynamoDBErrors::REQUEST_TIMEOUT,
            RetryExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR)),
        make_pair(
            DynamoDBErrors::PROVISIONED_THROUGHPUT_EXCEEDED,
            RetryExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR)),
        make_pair(
            DynamoDBErrors::REQUEST_LIMIT_EXCEEDED,
            RetryExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR)),
        make_pair(
            DynamoDBErrors::RESOURCE_IN_USE,
            RetryExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR)),
        make_pair(
            DynamoDBErrors::TABLE_IN_USE,
            RetryExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR)),
        make_pair(
            DynamoDBErrors::TRANSACTION_IN_PROGRESS,
            RetryExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::CONDITIONAL_CHECK_FAILED,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_CONDITIONAL_CHECKED_FAILED)),
        // Unretriable error codes,
        make_pair(DynamoDBErrors::INCOMPLETE_SIGNATURE,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::INVALID_ACTION,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::INVALID_CLIENT_TOKEN_ID,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::INVALID_PARAMETER_COMBINATION,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::INVALID_QUERY_PARAMETER,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::INVALID_PARAMETER_VALUE,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::MISSING_ACTION,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::MISSING_AUTHENTICATION_TOKEN,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::MISSING_PARAMETER,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::OPT_IN_REQUIRED,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::REQUEST_EXPIRED,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::VALIDATION,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::ACCESS_DENIED,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::RESOURCE_NOT_FOUND,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::UNRECOGNIZED_CLIENT,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::MALFORMED_QUERY_STRING,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::REQUEST_TIME_TOO_SKEWED,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::INVALID_SIGNATURE,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::SIGNATURE_DOES_NOT_MATCH,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::INVALID_ACCESS_KEY_ID,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::NETWORK_CONNECTION,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::UNKNOWN,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::BACKUP_IN_USE,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::BACKUP_NOT_FOUND,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),

        make_pair(DynamoDBErrors::CONTINUOUS_BACKUPS_UNAVAILABLE,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::GLOBAL_TABLE_ALREADY_EXISTS,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::GLOBAL_TABLE_NOT_FOUND,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::IDEMPOTENT_PARAMETER_MISMATCH,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::INDEX_NOT_FOUND,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::INVALID_RESTORE_TIME,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::ITEM_COLLECTION_SIZE_LIMIT_EXCEEDED,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::LIMIT_EXCEEDED,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::POINT_IN_TIME_RECOVERY_UNAVAILABLE,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::REPLICA_ALREADY_EXISTS,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::REPLICA_NOT_FOUND,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::TABLE_ALREADY_EXISTS,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::TABLE_NOT_FOUND,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::TRANSACTION_CANCELED,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR)),
        make_pair(DynamoDBErrors::TRANSACTION_CONFLICT,
                  FailureExecutionResult(
                      SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR))));

TEST(AwsDynamoDBUtilsTests, GetPartitionAndSortKeyValuesSuccess) {
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse> context;
  context.request = make_shared<GetDatabaseItemRequest>();
  auto& key = *context.request->mutable_key();
  key.mutable_partition_key()->set_name("part_key");
  key.mutable_partition_key()->set_value_int(1);
  {
    const auto key_container_or =
        AwsDynamoDBUtils::GetPartitionAndSortKeyValues(context);
    EXPECT_SUCCESS(key_container_or);
    EXPECT_EQ(key_container_or->partition_key_name, "part_key");
    EXPECT_EQ(key_container_or->partition_key_val, AttributeValue().SetN("1"));
    EXPECT_EQ(key_container_or->sort_key_name, std::nullopt);
    EXPECT_EQ(key_container_or->sort_key_val, std::nullopt);
  }

  {
    key.mutable_sort_key()->set_name("sort_key");
    key.mutable_sort_key()->set_value_float(1.5);
    const auto key_container_or =
        AwsDynamoDBUtils::GetPartitionAndSortKeyValues(context);
    EXPECT_SUCCESS(key_container_or);
    EXPECT_EQ(key_container_or->partition_key_name, "part_key");
    EXPECT_EQ(key_container_or->partition_key_val, AttributeValue().SetN("1"));
    EXPECT_THAT(key_container_or->sort_key_name, Optional(Eq("sort_key")));
    EXPECT_THAT(key_container_or->sort_key_val,
                Optional(AttributeValue().SetN("1.500000")));
  }
}

TEST(AwsDynamoDBUtilsTests, GetPartitionAndSortKeyValuesFailure) {
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse> context;
  context.request = make_shared<GetDatabaseItemRequest>();
  auto& key = *context.request->mutable_key();
  key.mutable_partition_key()->set_name("part_key");
  // No part key value set.
  EXPECT_THAT(AwsDynamoDBUtils::GetPartitionAndSortKeyValues(context),
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE)));

  key.mutable_partition_key()->set_value_int(1);
  key.mutable_sort_key()->set_name("sort_key");
  // No sort key value set.
  EXPECT_THAT(AwsDynamoDBUtils::GetPartitionAndSortKeyValues(context),
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE)));
}

TEST(AwsDynamoDBUtilsTests, GetConditionExpressionAndAddValuesToMapSuccess) {
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse> context;
  context.request = make_shared<GetDatabaseItemRequest>();
  auto* attr = context.request->add_required_attributes();
  attr->set_name("attr1");
  attr->set_value_int(1);
  attr = context.request->add_required_attributes();
  attr->set_name("attr2");
  attr->set_value_float(1.5);

  Map<String, AttributeValue> attribute_values;
  const auto condition_expression_or =
      AwsDynamoDBUtils::GetConditionExpressionAndAddValuesToMap(
          context, attribute_values);
  ASSERT_THAT(
      condition_expression_or,
      IsSuccessfulAndHolds("attr1= :attribute_0 and attr2= :attribute_1"));
  EXPECT_THAT(attribute_values,
              UnorderedElementsAre(
                  Pair(":attribute_0", AttributeValue().SetN("1")),
                  Pair(":attribute_1", AttributeValue().SetN("1.500000"))));
}

TEST(AwsDynamoDBUtilsTests, GetConditionExpressionAndAddValuesToMapFailure) {
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse> context;
  context.request = make_shared<GetDatabaseItemRequest>();
  auto* attr = context.request->add_required_attributes();
  attr->set_name("attr1");
  attr = context.request->add_required_attributes();
  attr->set_name("attr2");

  Map<String, AttributeValue> attribute_values;
  const auto condition_expression_or =
      AwsDynamoDBUtils::GetConditionExpressionAndAddValuesToMap(
          context, attribute_values);
  // Attr1 no value.
  ASSERT_THAT(condition_expression_or,
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE)));

  context.request->mutable_required_attributes(0)->set_value_int(1);
  // Attr2 no value.
  ASSERT_THAT(condition_expression_or,
              ResultIs(FailureExecutionResult(
                  SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE)));
}

}  // namespace google::scp::cpio::client_providers
