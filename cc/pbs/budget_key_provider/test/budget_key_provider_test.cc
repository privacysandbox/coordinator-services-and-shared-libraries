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

#include "pbs/budget_key_provider/src/budget_key_provider.h"

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <list>
#include <utility>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/serialization/src/serialization.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/async_context.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/journal_service/mock/mock_journal_service.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider.h"
#include "core/test/utils/conditional_wait.h"
#include "pbs/budget_key/mock/mock_budget_key_with_overrides.h"
#include "pbs/budget_key/src/budget_key.h"
#include "pbs/budget_key_provider/mock/mock_budget_key_provider.h"
#include "pbs/budget_key_provider/src/error_codes.h"
#include "pbs/budget_key_provider/src/proto/budget_key_provider.pb.h"
#include "pbs/interface/budget_key_interface.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncOperation;
using google::scp::core::BytesBuffer;
using google::scp::core::CheckpointLog;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::JournalLogRequest;
using google::scp::core::JournalLogResponse;
using google::scp::core::JournalLogStatus;
using google::scp::core::JournalServiceInterface;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::Serialization;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::journal_service::mock::MockJournalService;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProvider;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::MockAggregateMetric;
using google::scp::cpio::MockMetricClient;
using google::scp::pbs::BudgetKey;
using google::scp::pbs::BudgetKeyInterface;
using google::scp::pbs::budget_key::mock::MockBudgetKey;
using google::scp::pbs::budget_key_provider::mock::MockBudgetKeyProvider;
using google::scp::pbs::budget_key_provider::proto::BudgetKeyProviderLog;
using google::scp::pbs::budget_key_provider::proto::BudgetKeyProviderLog_1_0;
using google::scp::pbs::budget_key_provider::proto::OperationType;
using std::atomic;
using std::function;
using std::list;
using std::make_shared;
using std::move;
using std::pair;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::vector;

static constexpr Uuid kDefaultUuid = {0, 0};

static shared_ptr<MockAggregateMetric> mock_aggregate_metric =
    make_shared<MockAggregateMetric>();

namespace google::scp::pbs::test {

class BudgetKeyProviderTest : public ::testing::Test {
 protected:
  BudgetKeyProviderTest() {
    mock_journal_service_ = make_shared<MockJournalService>();
    journal_service_ = mock_journal_service_;
    mock_async_executor_ = make_shared<MockAsyncExecutor>();
    async_executor_ = mock_async_executor_;
    nosql_database_provider_ = make_shared<MockNoSQLDatabaseProvider>();
    mock_metric_client_ = make_shared<MockMetricClient>();
    mock_config_provider_ = make_shared<MockConfigProvider>();
    real_async_executor_ =
        make_shared<AsyncExecutor>(/*thread_count=*/2, /*queue_cap=*/1000);
    mock_budget_key_provider_ = make_shared<MockBudgetKeyProvider>(
        async_executor_, journal_service_, nosql_database_provider_,
        mock_metric_client_, mock_config_provider_);

    EXPECT_SUCCESS(real_async_executor_->Init());
    EXPECT_SUCCESS(real_async_executor_->Run());
  }

  ~BudgetKeyProviderTest() { EXPECT_SUCCESS(real_async_executor_->Stop()); }

  shared_ptr<MockMetricClient> mock_metric_client_;
  shared_ptr<MockConfigProvider> mock_config_provider_;
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider_;
  shared_ptr<MockAsyncExecutor> mock_async_executor_;
  shared_ptr<AsyncExecutorInterface> async_executor_;
  shared_ptr<MockJournalService> mock_journal_service_;
  shared_ptr<AsyncExecutorInterface> real_async_executor_;
  shared_ptr<JournalServiceInterface> journal_service_;
  shared_ptr<MockBudgetKeyProvider> mock_budget_key_provider_;
};

TEST_F(BudgetKeyProviderTest, RunShouldReloadAllUnloadedKeys) {
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  budget_key_provider_pair->budget_key = make_shared<BudgetKey>(
      budget_key_name, budget_key_id, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_,
      mock_aggregate_metric);
  budget_key_provider_pair->is_loaded = false;
  auto budget_key_pair = make_pair(*budget_key_name, budget_key_provider_pair);

  mock_budget_key_provider_->GetBudgetKeys()->Insert(budget_key_pair,
                                                     budget_key_provider_pair);

  // Total calls must become 1 since one of the transactions was done and budget
  // key map should load.
  size_t total_calls = 0;
  mock_async_executor_->schedule_for_mock = [&](const AsyncOperation& work,
                                                auto timestamp,
                                                auto& cancellation_callback) {
    total_calls++;
    return SuccessExecutionResult();
  };

  EXPECT_SUCCESS(mock_budget_key_provider_->Run());
  EXPECT_EQ(total_calls, 1);
}

TEST_F(BudgetKeyProviderTest, GetBudgetKey) {
  auto budget_key_name = make_shared<string>("budget_key_name");

  GetBudgetKeyRequest get_budget_key_request{.budget_key_name =
                                                 budget_key_name};

  shared_ptr<BudgetKeyProviderPair> loaded_budget_key_provider_pair;
  mock_budget_key_provider_->log_load_budget_key_into_cache_mock =
      [&](core::AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
              get_budget_key_context,
          std::shared_ptr<BudgetKeyProviderPair>& budget_key_provider_pair) {
        EXPECT_EQ(
            mock_budget_key_provider_->GetInternalBudgetKeys()->IsEvictable(
                *get_budget_key_context.request->budget_key_name),
            false);
        EXPECT_EQ(*get_budget_key_context.request->budget_key_name,
                  *budget_key_name);
        EXPECT_EQ(*budget_key_provider_pair->budget_key->GetName(),
                  *budget_key_name);
        loaded_budget_key_provider_pair = budget_key_provider_pair;
        return SuccessExecutionResult();
      };

  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context(
          make_shared<GetBudgetKeyRequest>(move(get_budget_key_request)),
          [](auto& context) {});

  auto result = mock_budget_key_provider_->GetBudgetKey(get_budget_key_context);
  EXPECT_SUCCESS(result);

  result = mock_budget_key_provider_->GetBudgetKey(get_budget_key_context);
  EXPECT_EQ(result, RetryExecutionResult(
                        core::errors::SC_BUDGET_KEY_PROVIDER_ENTRY_IS_LOADING));

  loaded_budget_key_provider_pair->needs_loader = true;
  result = mock_budget_key_provider_->GetBudgetKey(get_budget_key_context);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(loaded_budget_key_provider_pair->needs_loader.load(), false);

  loaded_budget_key_provider_pair->is_loaded = true;
  result = mock_budget_key_provider_->GetBudgetKey(get_budget_key_context);
  EXPECT_SUCCESS(result);

  mock_budget_key_provider_->GetInternalBudgetKeys()->MarkAsBeingDeleted(
      *loaded_budget_key_provider_pair->budget_key->GetName());
  result = mock_budget_key_provider_->GetBudgetKey(get_budget_key_context);
  EXPECT_EQ(
      result,
      RetryExecutionResult(
          core::errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED));
}

TEST_F(BudgetKeyProviderTest, LogLoadBudgetKeyIntoCache) {
  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context;
  get_budget_key_context.request = make_shared<GetBudgetKeyRequest>();
  get_budget_key_context.request->budget_key_name =
      make_shared<BudgetKeyName>("Budget_Key_Name");
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();
  auto mock_budget_key = make_shared<MockBudgetKey>(
      get_budget_key_context.request->budget_key_name, budget_key_id,
      async_executor_, journal_service_, nosql_database_provider_,
      mock_metric_client_, mock_config_provider_);

  budget_key_provider_pair->budget_key =
      static_pointer_cast<BudgetKeyInterface>(mock_budget_key);

  budget_key_provider_pair->is_loaded = false;

  mock_journal_service_->log_mock =
      [](AsyncContext<JournalLogRequest, JournalLogResponse>&) {
        return FailureExecutionResult(123);
      };

  // the return value is success since dispatcher is doing work.
  EXPECT_EQ(mock_budget_key_provider_->LogLoadBudgetKeyIntoCache(
                get_budget_key_context, budget_key_provider_pair),
            SuccessExecutionResult());

  mock_journal_service_->log_mock =
      [&](AsyncContext<JournalLogRequest, JournalLogResponse>&
              journal_log_context) {
        EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                      journal_log_context.request->data, kDefaultUuid),
                  SuccessExecutionResult());
        EXPECT_EQ(journal_log_context.request->log_status,
                  JournalLogStatus::Log);
        EXPECT_EQ(journal_log_context.request->component_id.high, 0xFFFFFFF1);
        EXPECT_EQ(journal_log_context.request->component_id.low, 0x00000002);
        return SuccessExecutionResult();
      };

  EXPECT_EQ(mock_budget_key_provider_->LogLoadBudgetKeyIntoCache(
                get_budget_key_context, budget_key_provider_pair),
            SuccessExecutionResult());
}

TEST_F(BudgetKeyProviderTest, LogLoadBudgetKeyIntoCacheInvalidLog) {
  auto bytes_buffer = make_shared<BytesBuffer>(1);
  EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED));
}

TEST_F(BudgetKeyProviderTest, LogLoadBudgetKeyIntoCacheInvalidLogVersion) {
  BudgetKeyProviderLog budget_key_provider_log;
  budget_key_provider_log.mutable_version()->set_major(110);
  budget_key_provider_log.mutable_version()->set_minor(12);

  size_t bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_provider_log.ByteSizeLong());
  EXPECT_EQ(Serialization::SerializeProtoMessage<BudgetKeyProviderLog>(
                *bytes_buffer, 0, budget_key_provider_log, bytes_serialized),
            SuccessExecutionResult());
  EXPECT_EQ(budget_key_provider_log.ByteSizeLong(), bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_SERIALIZATION_VERSION_IS_INVALID));
}

TEST_F(BudgetKeyProviderTest, LogLoadBudgetKeyIntoCacheInvalidLog1_0) {
  BudgetKeyProviderLog budget_key_provider_log;
  budget_key_provider_log.mutable_version()->set_major(1);
  budget_key_provider_log.mutable_version()->set_minor(0);

  BytesBuffer budget_key_provider_log_1_0(12);
  budget_key_provider_log.set_log_body(
      budget_key_provider_log_1_0.bytes->data(),
      budget_key_provider_log_1_0.capacity);

  size_t bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_provider_log.ByteSizeLong());
  EXPECT_EQ(Serialization::SerializeProtoMessage<BudgetKeyProviderLog>(
                *bytes_buffer, 0, budget_key_provider_log, bytes_serialized),
            SuccessExecutionResult());
  EXPECT_EQ(budget_key_provider_log.ByteSizeLong(), bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED));
}

TEST_F(BudgetKeyProviderTest, LogLoadBudgetKeyIntoCacheInvalidOperation) {
  BudgetKeyProviderLog budget_key_provider_log;
  budget_key_provider_log.mutable_version()->set_major(1);
  budget_key_provider_log.mutable_version()->set_minor(0);

  BudgetKeyProviderLog_1_0 budget_key_provider_log_1_0;
  budget_key_provider_log_1_0.mutable_id()->set_high(123);
  budget_key_provider_log_1_0.mutable_id()->set_low(456);
  budget_key_provider_log_1_0.set_budget_key_name("Budget_Key_Name");
  budget_key_provider_log_1_0.set_operation_type(
      OperationType::OPERATION_TYPE_UNKNOWN);

  BytesBuffer budget_key_provider_log_1_0_bytes_buffer(
      budget_key_provider_log_1_0.ByteSizeLong());
  size_t bytes_serialized = 0;
  Serialization::SerializeProtoMessage<BudgetKeyProviderLog_1_0>(
      budget_key_provider_log_1_0_bytes_buffer, 0, budget_key_provider_log_1_0,
      bytes_serialized);
  EXPECT_EQ(bytes_serialized, budget_key_provider_log_1_0.ByteSizeLong());
  budget_key_provider_log_1_0_bytes_buffer.length = bytes_serialized;

  budget_key_provider_log.set_log_body(
      budget_key_provider_log_1_0_bytes_buffer.bytes->data(),
      budget_key_provider_log_1_0_bytes_buffer.length);

  bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_provider_log.ByteSizeLong());
  EXPECT_EQ(Serialization::SerializeProtoMessage<BudgetKeyProviderLog>(
                *bytes_buffer, 0, budget_key_provider_log, bytes_serialized),
            SuccessExecutionResult());
  EXPECT_EQ(budget_key_provider_log.ByteSizeLong(), bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_BUDGET_KEY_PROVIDER_INVALID_OPERATION_TYPE));
}

TEST_F(BudgetKeyProviderTest, LogLoadBudgetKeyIntoCacheLoadIntoCache) {
  {
    BudgetKeyProviderLog budget_key_provider_log;
    budget_key_provider_log.mutable_version()->set_major(1);
    budget_key_provider_log.mutable_version()->set_minor(0);

    BudgetKeyProviderLog_1_0 budget_key_provider_log_1_0;
    budget_key_provider_log_1_0.mutable_id()->set_high(123);
    budget_key_provider_log_1_0.mutable_id()->set_low(456);
    budget_key_provider_log_1_0.set_budget_key_name("Budget_Key_Name");
    budget_key_provider_log_1_0.set_operation_type(
        OperationType::LOAD_INTO_CACHE);

    BytesBuffer budget_key_provider_log_1_0_bytes_buffer(
        budget_key_provider_log_1_0.ByteSizeLong());
    size_t bytes_serialized = 0;
    Serialization::SerializeProtoMessage<BudgetKeyProviderLog_1_0>(
        budget_key_provider_log_1_0_bytes_buffer, 0,
        budget_key_provider_log_1_0, bytes_serialized);
    EXPECT_EQ(bytes_serialized, budget_key_provider_log_1_0.ByteSizeLong());
    budget_key_provider_log_1_0_bytes_buffer.length = bytes_serialized;

    budget_key_provider_log.set_log_body(
        budget_key_provider_log_1_0_bytes_buffer.bytes->data(),
        budget_key_provider_log_1_0_bytes_buffer.length);

    bytes_serialized = 0;
    auto bytes_buffer =
        make_shared<BytesBuffer>(budget_key_provider_log.ByteSizeLong());
    EXPECT_EQ(Serialization::SerializeProtoMessage<BudgetKeyProviderLog>(
                  *bytes_buffer, 0, budget_key_provider_log, bytes_serialized),
              SuccessExecutionResult());
    EXPECT_EQ(budget_key_provider_log.ByteSizeLong(), bytes_serialized);
    bytes_buffer->length = bytes_serialized;

    EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                  bytes_buffer, kDefaultUuid),
              SuccessExecutionResult());
  }

  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair;
  EXPECT_EQ(mock_budget_key_provider_->GetBudgetKeys()->Find(
                "Budget_Key_Name", budget_key_provider_pair),
            SuccessExecutionResult());

  EXPECT_EQ(budget_key_provider_pair->is_loaded.load(), false);
  EXPECT_EQ(*budget_key_provider_pair->budget_key->GetName(),
            "Budget_Key_Name");
  EXPECT_EQ(budget_key_provider_pair->budget_key->GetId().high, 123);
  EXPECT_EQ(budget_key_provider_pair->budget_key->GetId().low, 456);

  // Calling again will replace the budget key if the id is the same
  {
    BudgetKeyProviderLog budget_key_provider_log;
    budget_key_provider_log.mutable_version()->set_major(1);
    budget_key_provider_log.mutable_version()->set_minor(0);

    BudgetKeyProviderLog_1_0 budget_key_provider_log_1_0;
    budget_key_provider_log_1_0.mutable_id()->set_high(789);
    budget_key_provider_log_1_0.mutable_id()->set_low(012);
    budget_key_provider_log_1_0.set_budget_key_name("Budget_Key_Name");
    budget_key_provider_log_1_0.set_operation_type(
        OperationType::LOAD_INTO_CACHE);

    BytesBuffer budget_key_provider_log_1_0_bytes_buffer(
        budget_key_provider_log_1_0.ByteSizeLong());
    size_t bytes_serialized = 0;
    Serialization::SerializeProtoMessage<BudgetKeyProviderLog_1_0>(
        budget_key_provider_log_1_0_bytes_buffer, 0,
        budget_key_provider_log_1_0, bytes_serialized);
    EXPECT_EQ(bytes_serialized, budget_key_provider_log_1_0.ByteSizeLong());
    budget_key_provider_log_1_0_bytes_buffer.length = bytes_serialized;

    budget_key_provider_log.set_log_body(
        budget_key_provider_log_1_0_bytes_buffer.bytes->data(),
        budget_key_provider_log_1_0_bytes_buffer.length);

    bytes_serialized = 0;
    auto bytes_buffer =
        make_shared<BytesBuffer>(budget_key_provider_log.ByteSizeLong());
    EXPECT_EQ(Serialization::SerializeProtoMessage<BudgetKeyProviderLog>(
                  *bytes_buffer, 0, budget_key_provider_log, bytes_serialized),
              SuccessExecutionResult());
    EXPECT_EQ(budget_key_provider_log.ByteSizeLong(), bytes_serialized);
    bytes_buffer->length = bytes_serialized;

    EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                  bytes_buffer, kDefaultUuid),
              FailureExecutionResult(
                  core::errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS));
  }

  {
    BudgetKeyProviderLog budget_key_provider_log;
    budget_key_provider_log.mutable_version()->set_major(1);
    budget_key_provider_log.mutable_version()->set_minor(0);

    BudgetKeyProviderLog_1_0 budget_key_provider_log_1_0;
    budget_key_provider_log_1_0.mutable_id()->set_high(123);
    budget_key_provider_log_1_0.mutable_id()->set_low(456);
    budget_key_provider_log_1_0.set_budget_key_name("Budget_Key_Name");
    budget_key_provider_log_1_0.set_operation_type(
        OperationType::LOAD_INTO_CACHE);

    BytesBuffer budget_key_provider_log_1_0_bytes_buffer(
        budget_key_provider_log_1_0.ByteSizeLong());
    size_t bytes_serialized = 0;
    Serialization::SerializeProtoMessage<BudgetKeyProviderLog_1_0>(
        budget_key_provider_log_1_0_bytes_buffer, 0,
        budget_key_provider_log_1_0, bytes_serialized);
    EXPECT_EQ(bytes_serialized, budget_key_provider_log_1_0.ByteSizeLong());
    budget_key_provider_log_1_0_bytes_buffer.length = bytes_serialized;

    budget_key_provider_log.set_log_body(
        budget_key_provider_log_1_0_bytes_buffer.bytes->data(),
        budget_key_provider_log_1_0_bytes_buffer.length);

    bytes_serialized = 0;
    auto bytes_buffer =
        make_shared<BytesBuffer>(budget_key_provider_log.ByteSizeLong());
    EXPECT_EQ(Serialization::SerializeProtoMessage<BudgetKeyProviderLog>(
                  *bytes_buffer, 0, budget_key_provider_log, bytes_serialized),
              SuccessExecutionResult());
    EXPECT_EQ(budget_key_provider_log.ByteSizeLong(), bytes_serialized);
    bytes_buffer->length = bytes_serialized;

    EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                  bytes_buffer, kDefaultUuid),
              SuccessExecutionResult());
  }

  EXPECT_EQ(budget_key_provider_pair->is_loaded.load(), false);
  EXPECT_EQ(*budget_key_provider_pair->budget_key->GetName(),
            "Budget_Key_Name");
  EXPECT_EQ(budget_key_provider_pair->budget_key->GetId().high, 123);
  EXPECT_EQ(budget_key_provider_pair->budget_key->GetId().low, 456);
}

TEST_F(BudgetKeyProviderTest, LogLoadBudgetKeyIntoCacheDeleteOperation) {
  BudgetKeyProviderLog budget_key_provider_log;
  budget_key_provider_log.mutable_version()->set_major(1);
  budget_key_provider_log.mutable_version()->set_minor(0);

  BudgetKeyProviderLog_1_0 budget_key_provider_log_1_0;
  budget_key_provider_log_1_0.mutable_id()->set_high(123);
  budget_key_provider_log_1_0.mutable_id()->set_low(456);
  budget_key_provider_log_1_0.set_budget_key_name("Budget_Key_Name");
  budget_key_provider_log_1_0.set_operation_type(
      OperationType::DELETE_FROM_CACHE);

  BytesBuffer budget_key_provider_log_1_0_bytes_buffer(
      budget_key_provider_log_1_0.ByteSizeLong());
  size_t bytes_serialized = 0;
  Serialization::SerializeProtoMessage<BudgetKeyProviderLog_1_0>(
      budget_key_provider_log_1_0_bytes_buffer, 0, budget_key_provider_log_1_0,
      bytes_serialized);
  EXPECT_EQ(bytes_serialized, budget_key_provider_log_1_0.ByteSizeLong());
  budget_key_provider_log_1_0_bytes_buffer.length = bytes_serialized;

  budget_key_provider_log.set_log_body(
      budget_key_provider_log_1_0_bytes_buffer.bytes->data(),
      budget_key_provider_log_1_0_bytes_buffer.length);

  bytes_serialized = 0;
  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_provider_log.ByteSizeLong());
  EXPECT_EQ(Serialization::SerializeProtoMessage<BudgetKeyProviderLog>(
                *bytes_buffer, 0, budget_key_provider_log, bytes_serialized),
            SuccessExecutionResult());
  EXPECT_EQ(budget_key_provider_log.ByteSizeLong(), bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            SuccessExecutionResult());

  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto pair = make_pair(budget_key_provider_log_1_0.budget_key_name(),
                        budget_key_provider_pair);
  mock_budget_key_provider_->GetBudgetKeys()->Insert(pair,
                                                     budget_key_provider_pair);

  EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            SuccessExecutionResult());
}

TEST_F(BudgetKeyProviderTest, OnLogLoadBudgetKeyIntoCacheCallbackFailure) {
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  budget_key_provider_pair->budget_key = make_shared<BudgetKey>(
      budget_key_name, budget_key_id, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_,
      mock_aggregate_metric);
  budget_key_provider_pair->is_loaded = false;
  auto budget_key_pair = make_pair(*budget_key_name, budget_key_provider_pair);

  mock_budget_key_provider_->GetBudgetKeys()->Insert(budget_key_pair,
                                                     budget_key_provider_pair);

  mock_budget_key_provider_->GetInternalBudgetKeys()->DisableEviction(
      budget_key_pair.first);
  EXPECT_EQ(mock_budget_key_provider_->GetInternalBudgetKeys()->IsEvictable(
                budget_key_pair.first),
            false);

  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context;

  get_budget_key_context.callback =
      [&](AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
              get_budget_key_context) {
        EXPECT_THAT(get_budget_key_context.result,
                    ResultIs(FailureExecutionResult(123)));
        EXPECT_EQ(mock_budget_key_provider_->GetBudgetKeys()->Find(
                      *budget_key_name, budget_key_provider_pair),
                  SuccessExecutionResult());
        EXPECT_EQ(budget_key_provider_pair->needs_loader.load(), true);
        EXPECT_EQ(budget_key_provider_pair->is_loaded.load(), false);
        EXPECT_EQ(
            mock_budget_key_provider_->GetInternalBudgetKeys()->IsEvictable(
                *budget_key_name),
            true);
      };

  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;

  journal_log_context.result = FailureExecutionResult(123);
  mock_budget_key_provider_->OnLogLoadBudgetKeyIntoCacheCallback(
      get_budget_key_context, budget_key_provider_pair, journal_log_context);
}

TEST_F(BudgetKeyProviderTest, OnLogLoadBudgetKeyIntoCacheCallbackRetry) {
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  budget_key_provider_pair->budget_key = make_shared<BudgetKey>(
      budget_key_name, budget_key_id, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_,
      mock_aggregate_metric);
  budget_key_provider_pair->is_loaded = false;
  auto budget_key_pair = make_pair(*budget_key_name, budget_key_provider_pair);

  mock_budget_key_provider_->GetBudgetKeys()->Insert(budget_key_pair,
                                                     budget_key_provider_pair);

  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context;

  get_budget_key_context.callback =
      [&](AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
              get_budget_key_context) {
        EXPECT_THAT(get_budget_key_context.result,
                    ResultIs(RetryExecutionResult(123)));
        EXPECT_EQ(mock_budget_key_provider_->GetBudgetKeys()->Find(
                      *budget_key_name, budget_key_provider_pair),
                  SuccessExecutionResult());
        EXPECT_EQ(budget_key_provider_pair->needs_loader.load(), true);
        EXPECT_EQ(budget_key_provider_pair->is_loaded.load(), false);
      };

  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;

  journal_log_context.result = RetryExecutionResult(123);
  mock_budget_key_provider_->OnLogLoadBudgetKeyIntoCacheCallback(
      get_budget_key_context, budget_key_provider_pair, journal_log_context);
}

TEST_F(BudgetKeyProviderTest, OnLogLoadBudgetKeyIntoCacheCallbackSuccess) {
  auto budget_key_name = make_shared<BudgetKeyName>("Budget_Key_Name");
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();
  auto budget_key = make_shared<MockBudgetKey>(
      budget_key_name, budget_key_id, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_);

  budget_key_provider_pair->budget_key = budget_key;
  budget_key_provider_pair->is_loaded = false;

  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context;

  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.result = SuccessExecutionResult();

  auto condition = false;
  budget_key->load_budget_key_mock =
      [&](core::AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&) {
        condition = true;
        return SuccessExecutionResult();
      };

  mock_budget_key_provider_->OnLogLoadBudgetKeyIntoCacheCallback(
      get_budget_key_context, budget_key_provider_pair, journal_log_context);
  EXPECT_EQ(condition, true);
}

TEST_F(BudgetKeyProviderTest, OnLoadBudgetKeyCallbackFailure) {
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  budget_key_provider_pair->budget_key = make_shared<BudgetKey>(
      budget_key_name, budget_key_id, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_,
      mock_aggregate_metric);
  budget_key_provider_pair->is_loaded = false;
  auto budget_key_pair = make_pair(*budget_key_name, budget_key_provider_pair);

  mock_budget_key_provider_->GetBudgetKeys()->Insert(budget_key_pair,
                                                     budget_key_provider_pair);
  mock_budget_key_provider_->GetInternalBudgetKeys()->DisableEviction(
      budget_key_pair.first);
  EXPECT_EQ(mock_budget_key_provider_->GetInternalBudgetKeys()->IsEvictable(
                budget_key_pair.first),
            false);

  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context;

  get_budget_key_context.callback =
      [&](AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
              get_budget_key_context) {
        EXPECT_THAT(get_budget_key_context.result,
                    ResultIs(FailureExecutionResult(123)));
        EXPECT_EQ(mock_budget_key_provider_->GetBudgetKeys()->Find(
                      *budget_key_name, budget_key_provider_pair),
                  SuccessExecutionResult());
        EXPECT_EQ(budget_key_provider_pair->needs_loader.load(), true);
        EXPECT_EQ(budget_key_provider_pair->is_loaded.load(), false);
        EXPECT_EQ(
            mock_budget_key_provider_->GetInternalBudgetKeys()->IsEvictable(
                budget_key_pair.first),
            true);
      };

  AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>
      load_budget_key_context;

  load_budget_key_context.result = FailureExecutionResult(123);
  mock_budget_key_provider_->OnLoadBudgetKeyCallback(get_budget_key_context,
                                                     budget_key_provider_pair,
                                                     load_budget_key_context);
  EXPECT_EQ(budget_key_provider_pair->needs_loader.load(), true);
}

TEST_F(BudgetKeyProviderTest, OnLoadBudgetKeyCallbackRetry) {
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  budget_key_provider_pair->budget_key = make_shared<BudgetKey>(
      budget_key_name, budget_key_id, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_,
      mock_aggregate_metric);
  budget_key_provider_pair->is_loaded = false;
  auto budget_key_pair = make_pair(*budget_key_name, budget_key_provider_pair);

  mock_budget_key_provider_->GetBudgetKeys()->Insert(budget_key_pair,
                                                     budget_key_provider_pair);

  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context;

  get_budget_key_context.callback =
      [&](AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
              get_budget_key_context) {
        EXPECT_THAT(get_budget_key_context.result,
                    ResultIs(RetryExecutionResult(123)));
        EXPECT_EQ(mock_budget_key_provider_->GetBudgetKeys()->Find(
                      *budget_key_name, budget_key_provider_pair),
                  SuccessExecutionResult());
        EXPECT_EQ(budget_key_provider_pair->needs_loader.load(), true);
        EXPECT_EQ(budget_key_provider_pair->is_loaded.load(), false);
      };

  AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>
      load_budget_key_context;

  load_budget_key_context.result = RetryExecutionResult(123);
  mock_budget_key_provider_->OnLoadBudgetKeyCallback(get_budget_key_context,
                                                     budget_key_provider_pair,
                                                     load_budget_key_context);
}

TEST_F(BudgetKeyProviderTest, OnLoadBudgetKeyCallbackSuccess) {
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key_name");

  budget_key_provider_pair->budget_key = make_shared<MockBudgetKey>(
      budget_key_name, budget_key_id, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_);

  budget_key_provider_pair->is_loaded = false;
  auto budget_key_pair = make_pair(*budget_key_name, budget_key_provider_pair);

  mock_budget_key_provider_->GetBudgetKeys()->Insert(budget_key_pair,
                                                     budget_key_provider_pair);

  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context;

  get_budget_key_context.callback =
      [&](AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>&
              get_budget_key_context) {
        EXPECT_SUCCESS(get_budget_key_context.result);
        EXPECT_EQ(mock_budget_key_provider_->GetBudgetKeys()->Find(
                      *budget_key_name, budget_key_provider_pair),
                  SuccessExecutionResult());
        EXPECT_EQ(budget_key_provider_pair->is_loaded, true);
        EXPECT_EQ(*get_budget_key_context.response->budget_key->GetName(),
                  *budget_key_name);
        EXPECT_EQ(get_budget_key_context.response->budget_key->GetId(),
                  budget_key_id);
      };

  AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>
      load_budget_key_context;

  load_budget_key_context.result = SuccessExecutionResult();
  mock_budget_key_provider_->OnLoadBudgetKeyCallback(get_budget_key_context,
                                                     budget_key_provider_pair,
                                                     load_budget_key_context);
}

TEST_F(BudgetKeyProviderTest, OnBeforeGarbageCollection) {
  AsyncContext<GetBudgetKeyRequest, GetBudgetKeyResponse>
      get_budget_key_context;
  get_budget_key_context.request = make_shared<GetBudgetKeyRequest>();
  get_budget_key_context.request->budget_key_name =
      make_shared<BudgetKeyName>("Budget_Key_Name");
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();

  auto mock_budget_key = make_shared<MockBudgetKey>(
      get_budget_key_context.request->budget_key_name, budget_key_id,
      async_executor_, journal_service_, nosql_database_provider_,
      mock_metric_client_, mock_config_provider_);

  budget_key_provider_pair->budget_key =
      static_pointer_cast<BudgetKeyInterface>(mock_budget_key);

  budget_key_provider_pair->is_loaded = true;

  mock_journal_service_->log_mock =
      [&](AsyncContext<JournalLogRequest, JournalLogResponse>&
              journal_log_context) {
        EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                      journal_log_context.request->data, kDefaultUuid),
                  SuccessExecutionResult());
        EXPECT_EQ(journal_log_context.request->log_status,
                  JournalLogStatus::Log);
        EXPECT_EQ(journal_log_context.request->component_id.high, 0xFFFFFFF1);
        EXPECT_EQ(journal_log_context.request->component_id.low, 0x00000002);
        return SuccessExecutionResult();
      };

  auto pair = make_pair(*get_budget_key_context.request->budget_key_name,
                        budget_key_provider_pair);
  mock_budget_key_provider_->GetBudgetKeys()->Insert(pair,
                                                     budget_key_provider_pair);

  auto should_delete = [](bool should_delete) {};
  auto budget_key_name = *budget_key_provider_pair->budget_key->GetName();
  mock_budget_key_provider_->OnBeforeGarbageCollection(
      budget_key_name, budget_key_provider_pair, should_delete);
}

TEST_F(BudgetKeyProviderTest, OnRemoveEntryFromCacheLogged) {
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<string>("budget_key_name");

  auto mock_budget_key = make_shared<MockBudgetKey>(
      budget_key_name, budget_key_id, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_);

  budget_key_provider_pair->budget_key =
      static_pointer_cast<BudgetKeyInterface>(mock_budget_key);
  budget_key_provider_pair->is_loaded = true;

  AsyncContext<JournalLogRequest, JournalLogResponse> journal_context;

  journal_context.result = FailureExecutionResult(123);
  function<void(bool)> should_delete = [](bool should_delete) {
    EXPECT_EQ(should_delete, false);
  };
  mock_budget_key_provider_->OnRemoveEntryFromCacheLogged(
      should_delete, budget_key_provider_pair, journal_context);

  journal_context.result = RetryExecutionResult(123);
  should_delete = [](bool should_delete) { EXPECT_EQ(should_delete, false); };
  mock_budget_key_provider_->OnRemoveEntryFromCacheLogged(
      should_delete, budget_key_provider_pair, journal_context);

  journal_context.result = SuccessExecutionResult();
  should_delete = [](bool should_delete) { EXPECT_EQ(should_delete, true); };
  mock_budget_key_provider_->OnRemoveEntryFromCacheLogged(
      should_delete, budget_key_provider_pair, journal_context);
}

TEST_F(BudgetKeyProviderTest, SerializeBudgetKeyProviderPair) {
  auto budget_key_name = make_shared<BudgetKeyName>("Budget_Key_Name");
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();

  auto mock_budget_key = make_shared<MockBudgetKey>(
      budget_key_name, budget_key_id, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_);

  budget_key_provider_pair->budget_key =
      static_pointer_cast<BudgetKeyInterface>(mock_budget_key);

  budget_key_provider_pair->is_loaded = true;

  auto bytes_buffer = make_shared<BytesBuffer>();
  EXPECT_EQ(mock_budget_key_provider_->SerializeBudgetKeyProviderPair(
                budget_key_provider_pair, OperationType::DELETE_FROM_CACHE,
                *bytes_buffer),
            SuccessExecutionResult());

  // Repeated logs are allowed and need to be ignored.
  EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            SuccessExecutionResult());

  EXPECT_EQ(mock_budget_key_provider_->SerializeBudgetKeyProviderPair(
                budget_key_provider_pair, OperationType::LOAD_INTO_CACHE,
                *bytes_buffer),
            SuccessExecutionResult());

  EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            SuccessExecutionResult());

  EXPECT_EQ(mock_budget_key_provider_->SerializeBudgetKeyProviderPair(
                budget_key_provider_pair, OperationType::DELETE_FROM_CACHE,
                *bytes_buffer),
            SuccessExecutionResult());

  EXPECT_EQ(mock_budget_key_provider_->OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            SuccessExecutionResult());
}

TEST_F(BudgetKeyProviderTest, Checkpoint) {
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();

  auto checkpoint_logs = make_shared<list<CheckpointLog>>();

  EXPECT_EQ(mock_budget_key_provider_->Checkpoint(checkpoint_logs),
            SuccessExecutionResult());
  EXPECT_EQ(checkpoint_logs->size(), 0);

  bool checkpoint_1_called = false;
  auto budget_key_id_1 = Uuid::GenerateUuid();
  auto budget_key_name_1 = make_shared<string>("budget_key_name_1");

  auto mock_budget_key_1 = make_shared<MockBudgetKey>(
      budget_key_name_1, budget_key_id_1, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_);
  mock_budget_key_1->checkpoint_mock = [&](shared_ptr<list<CheckpointLog>>&) {
    checkpoint_1_called = true;
    return SuccessExecutionResult();
  };
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair_1 =
      make_shared<BudgetKeyProviderPair>();
  budget_key_provider_pair_1->budget_key = mock_budget_key_1;
  budget_key_provider_pair_1->is_loaded = false;
  auto budget_key_pair_1 =
      make_pair(*budget_key_name_1, budget_key_provider_pair_1);
  mock_budget_key_provider_->GetBudgetKeys()->Insert(
      budget_key_pair_1, budget_key_provider_pair_1);

  auto budget_key_id_2 = Uuid::GenerateUuid();
  auto budget_key_name_2 = make_shared<string>("budget_key_name_2");
  auto mock_budget_key_2 = make_shared<MockBudgetKey>(
      budget_key_name_2, budget_key_id_2, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_);

  bool checkpoint_2_called = false;
  mock_budget_key_2->checkpoint_mock = [&](shared_ptr<list<CheckpointLog>>&) {
    checkpoint_2_called = true;
    return SuccessExecutionResult();
  };
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair_2 =
      make_shared<BudgetKeyProviderPair>();
  budget_key_provider_pair_2->budget_key = mock_budget_key_2;
  budget_key_provider_pair_2->is_loaded = false;
  auto budget_key_pair_2 =
      make_pair(*budget_key_name_2, budget_key_provider_pair_2);
  mock_budget_key_provider_->GetBudgetKeys()->Insert(
      budget_key_pair_2, budget_key_provider_pair_2);

  EXPECT_EQ(mock_budget_key_provider_->Checkpoint(checkpoint_logs),
            SuccessExecutionResult());
  WaitUntil([&]() { return checkpoint_1_called && checkpoint_2_called; });

  EXPECT_EQ(checkpoint_logs->size(), 2);

  auto recovery_budget_key_provider = make_shared<MockBudgetKeyProvider>(
      async_executor_, journal_service_, nosql_database_provider_,
      mock_metric_client_, mock_config_provider_);

  Uuid budget_key_provider_id = {.high = 0xFFFFFFF1, .low = 0x00000002};
  auto it = checkpoint_logs->begin();
  EXPECT_EQ(it->component_id, budget_key_provider_id);
  EXPECT_NE(it->log_id.low, 0);
  EXPECT_NE(it->log_id.high, 0);
  EXPECT_EQ(it->log_status, JournalLogStatus::Log);

  auto bytes_buffer = make_shared<BytesBuffer>(it->bytes_buffer);
  EXPECT_EQ(recovery_budget_key_provider->OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            SuccessExecutionResult());

  it++;

  EXPECT_EQ(it->component_id, budget_key_provider_id);
  EXPECT_NE(it->log_id.low, 0);
  EXPECT_NE(it->log_id.high, 0);
  EXPECT_EQ(it->log_status, JournalLogStatus::Log);

  bytes_buffer = make_shared<BytesBuffer>(it->bytes_buffer);
  EXPECT_EQ(recovery_budget_key_provider->OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            SuccessExecutionResult());

  vector<string> budget_keys;
  vector<string> checkpoint_budget_keys;
  mock_budget_key_provider_->GetBudgetKeys()->Keys(budget_keys);
  recovery_budget_key_provider->GetBudgetKeys()->Keys(checkpoint_budget_keys);

  EXPECT_EQ(budget_keys.size(), 2);
  EXPECT_EQ(checkpoint_budget_keys.size(), 2);

  EXPECT_EQ(budget_keys[0] == checkpoint_budget_keys[0] ||
                budget_keys[1] == checkpoint_budget_keys[0],
            true);
  EXPECT_EQ(budget_keys[0] == checkpoint_budget_keys[1] ||
                budget_keys[1] == checkpoint_budget_keys[1],
            true);

  for (auto budget_key : budget_keys) {
    shared_ptr<BudgetKeyProviderPair> original_budget_key_provider_pair;
    shared_ptr<BudgetKeyProviderPair> checkpoint_budget_key_provider_pair;

    mock_budget_key_provider_->GetBudgetKeys()->Find(
        budget_key, original_budget_key_provider_pair);
    recovery_budget_key_provider->GetBudgetKeys()->Find(
        budget_key, checkpoint_budget_key_provider_pair);

    EXPECT_EQ(original_budget_key_provider_pair->is_loaded.load(),
              checkpoint_budget_key_provider_pair->is_loaded.load());
    EXPECT_EQ(original_budget_key_provider_pair->budget_key->GetId(),
              checkpoint_budget_key_provider_pair->budget_key->GetId());
  }
}

TEST_F(BudgetKeyProviderTest, CheckpointFailureOnBudgetKeyCheckpoint) {
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();

  auto checkpoint_logs = make_shared<list<CheckpointLog>>();

  EXPECT_EQ(mock_budget_key_provider_->Checkpoint(checkpoint_logs),
            SuccessExecutionResult());
  EXPECT_EQ(checkpoint_logs->size(), 0);

  auto budget_key_id_1 = Uuid::GenerateUuid();
  auto budget_key_name_1 = make_shared<string>("budget_key_name_1");
  auto mock_budget_key_1 = make_shared<MockBudgetKey>(
      budget_key_name_1, budget_key_id_1, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_);
  mock_budget_key_1->checkpoint_mock = [&](shared_ptr<list<CheckpointLog>>&) {
    return FailureExecutionResult(1234);
  };
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair_1 =
      make_shared<BudgetKeyProviderPair>();
  budget_key_provider_pair_1->budget_key = mock_budget_key_1;
  budget_key_provider_pair_1->is_loaded = false;
  auto budget_key_pair_1 =
      make_pair(*budget_key_name_1, budget_key_provider_pair_1);
  mock_budget_key_provider_->GetBudgetKeys()->Insert(
      budget_key_pair_1, budget_key_provider_pair_1);

  EXPECT_EQ(mock_budget_key_provider_->Checkpoint(checkpoint_logs),
            FailureExecutionResult(1234));
}

TEST_F(BudgetKeyProviderTest, StopShouldFailIfCannotStopBudgetKey) {
  auto budget_key_provider_pair = make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key");
  auto mock_budget_key = make_shared<MockBudgetKey>(
      budget_key_name, budget_key_id, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_);
  budget_key_provider_pair->budget_key = mock_budget_key;
  auto budget_key_pair_to_insert =
      make_pair(*budget_key_name, budget_key_provider_pair);
  auto out = make_shared<BudgetKeyProviderPair>();
  auto budget_key_provider = make_shared<MockBudgetKeyProvider>(
      real_async_executor_, journal_service_, nosql_database_provider_,
      mock_metric_client_, mock_config_provider_);
  budget_key_provider->GetBudgetKeys()->Insert(budget_key_pair_to_insert, out);

  // Return Failure
  mock_budget_key->stop_mock = []() { return FailureExecutionResult(1234); };

  EXPECT_SUCCESS(budget_key_provider->Init());
  EXPECT_SUCCESS(budget_key_provider->Run());
  EXPECT_THAT(budget_key_provider->Stop(),
              ResultIs(FailureExecutionResult(1234)));
}

TEST_F(BudgetKeyProviderTest, StopShouldSucceedIfCanStopBudgetKey) {
  auto budget_key_provider_pair = make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();
  auto budget_key_name = make_shared<BudgetKeyName>("budget_key");
  auto mock_budget_key = make_shared<MockBudgetKey>(
      budget_key_name, budget_key_id, async_executor_, journal_service_,
      nosql_database_provider_, mock_metric_client_, mock_config_provider_);
  budget_key_provider_pair->budget_key = mock_budget_key;
  auto budget_key_pair_to_insert =
      make_pair(*budget_key_name, budget_key_provider_pair);
  auto out = make_shared<BudgetKeyProviderPair>();
  auto budget_key_provider = make_shared<MockBudgetKeyProvider>(
      real_async_executor_, journal_service_, nosql_database_provider_,
      mock_metric_client_, mock_config_provider_);
  budget_key_provider->GetBudgetKeys()->Insert(budget_key_pair_to_insert, out);

  // Return Success
  mock_budget_key->stop_mock = []() { return SuccessExecutionResult(); };

  EXPECT_SUCCESS(budget_key_provider->Init());
  EXPECT_SUCCESS(budget_key_provider->Run());
  EXPECT_SUCCESS(budget_key_provider->Stop());
}
}  // namespace google::scp::pbs::test
