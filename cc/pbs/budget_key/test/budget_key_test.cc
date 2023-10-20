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

#include "pbs/budget_key/src/budget_key.h"

#include <gtest/gtest.h>

#include <atomic>
#include <list>
#include <utility>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/common/serialization/src/error_codes.h"
#include "core/common/serialization/src/serialization.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/async_context.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/journal_service/mock/mock_journal_service.h"
#include "core/journal_service/mock/mock_journal_service_with_overrides.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider.h"
#include "core/test/utils/conditional_wait.h"
#include "pbs/budget_key/mock/mock_budget_key_with_overrides.h"
#include "pbs/budget_key/src/proto/budget_key.pb.h"
#include "pbs/budget_key_timeframe_manager/mock/mock_budget_key_timeframe_manager.h"
#include "pbs/budget_key_transaction_protocols/mock/mock_consume_budget_transaction_protocol.h"
#include "pbs/interface/configuration_keys.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"
#include "public/cpio/utils/metric_aggregation/mock/mock_aggregate_metric.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::BlobStorageProviderInterface;
using google::scp::core::BytesBuffer;
using google::scp::core::CheckpointLog;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::JournalLogRequest;
using google::scp::core::JournalLogResponse;
using google::scp::core::JournalLogStatus;
using google::scp::core::JournalServiceInterface;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::OnLogRecoveredCallback;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::Serialization;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::journal_service::mock::MockJournalService;
using google::scp::core::journal_service::mock::MockJournalServiceWithOverrides;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProvider;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::MockAggregateMetric;
using google::scp::cpio::MockMetricClient;
using google::scp::pbs::BudgetKey;
using google::scp::pbs::budget_key::mock::MockBudgetKey;
using google::scp::pbs::budget_key::mock::MockConsumeBudgetTransactionProtocol;
using google::scp::pbs::budget_key::proto::BudgetKeyLog;
using google::scp::pbs::budget_key::proto::BudgetKeyLog_1_0;
using google::scp::pbs::buget_key_timeframe_manager::mock::
    MockBudgetKeyTimeframeManager;
using std::atomic;
using std::list;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::vector;

static constexpr Uuid kDefaultUuid = {0, 0};

static shared_ptr<MockAggregateMetric> mock_aggregate_metric =
    make_shared<MockAggregateMetric>();

namespace google::scp::pbs::test {
TEST(BudgetKeyTest, InitShouldSubscribe) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider;

  auto budget_key_name = make_shared<string>("test_budget_key");
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_journal_service = make_shared<MockJournalServiceWithOverrides>(
      bucket_name, partition_name, async_executor, blob_storage_provider,
      mock_metric_client, mock_config_provider);
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);

  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider;
  BudgetKey budget_key(budget_key_name, Uuid::GenerateUuid(), async_executor,
                       journal_service, nosql_database_provider,
                       budget_key_manager, budget_key_transaction_protocol,
                       mock_metric_client, mock_config_provider,
                       mock_aggregate_metric);

  OnLogRecoveredCallback callback;
  EXPECT_EQ(mock_journal_service->GetSubscribersMap().Find(budget_key.GetId(),
                                                           callback),
            FailureExecutionResult(
                core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));

  budget_key.Init();

  EXPECT_EQ(mock_journal_service->GetSubscribersMap().Find(budget_key.GetId(),
                                                           callback),
            SuccessExecutionResult());
}

TEST(BudgetKeyTest, GetBudget) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();
  core::Timestamp reporting_time = 10;
  shared_ptr<JournalServiceInterface> journal_service;

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(1)};

  for (auto result : results) {
    atomic<bool> condition(false);
    budget_key_manager->load_function =
        [&](auto& load_budget_key_timeframe_context) {
          EXPECT_EQ(
              load_budget_key_timeframe_context.request->reporting_times.size(),
              1);
          EXPECT_EQ(
              load_budget_key_timeframe_context.request->reporting_times[0],
              reporting_time);

          load_budget_key_timeframe_context.result = result;
          load_budget_key_timeframe_context.Finish();
          return result;
        };

    shared_ptr<AsyncExecutorInterface> async_executor =
        make_shared<MockAsyncExecutor>();
    shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider;
    auto mock_metric_client = make_shared<MockMetricClient>();
    auto mock_config_provider = make_shared<MockConfigProvider>();
    BudgetKey budget_key(budget_key_name, Uuid::GenerateUuid(), async_executor,
                         journal_service, nosql_database_provider,
                         budget_key_manager, budget_key_transaction_protocol,
                         mock_metric_client, mock_config_provider,
                         mock_aggregate_metric);
    GetBudgetRequest request = {.time_bucket = reporting_time};
    AsyncContext<GetBudgetRequest, GetBudgetResponse> get_budget_context(
        make_shared<GetBudgetRequest>(move(request)), [](auto& context) {});
    get_budget_context.callback = [result,
                                   &condition](auto get_budget_context) {
      EXPECT_THAT(get_budget_context.result, ResultIs(result));
      if (result.Successful()) {
        EXPECT_EQ(get_budget_context.response->token_count, kMaxToken);
      }
      condition = true;
      return result;
    };

    budget_key.GetBudget(get_budget_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BudgetKeyTest, LoadBudgetKey) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  auto budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  shared_ptr<JournalServiceInterface> journal_service =
      make_shared<MockJournalService>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider;
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  BudgetKey budget_key(budget_key_name, Uuid::GenerateUuid(), async_executor,
                       journal_service, nosql_database_provider,
                       budget_key_manager, budget_key_transaction_protocol,
                       mock_metric_client, mock_config_provider,
                       mock_aggregate_metric);

  AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>
      load_budget_key_context;
  load_budget_key_context.callback =
      [&](AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
              load_budget_key_context) {
        EXPECT_SUCCESS(load_budget_key_context.result);
      };

  EXPECT_EQ(budget_key.LoadBudgetKey(load_budget_key_context),
            SuccessExecutionResult());
}

TEST(BudgetKeyTest, LoadBudgetKeyWithSerialization) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  shared_ptr<BudgetKeyTimeframeManagerInterface> budget_key_manager = nullptr;
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  mock_config_provider->Set(kBudgetKeyTableName, string("PBS_BudgetKeys"));
  MockBudgetKey budget_key(
      budget_key_name, Uuid::GenerateUuid(), async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  mock_journal_service
      ->log_mock = [&](AsyncContext<JournalLogRequest, JournalLogResponse>&
                           journal_log_context) mutable {
    EXPECT_EQ(journal_log_context.request->log_status, JournalLogStatus::Log);
    EXPECT_EQ(journal_log_context.request->component_id.high,
              budget_key.GetId().high);
    EXPECT_EQ(journal_log_context.request->component_id.low,
              budget_key.GetId().low);
    EXPECT_EQ(journal_log_context.request->log_status, JournalLogStatus::Log);
    EXPECT_NE(journal_log_context.request->data->bytes->size(), 0);
    EXPECT_NE(journal_log_context.request->data->capacity, 0);
    EXPECT_NE(journal_log_context.request->data->length, 0);

    MockBudgetKey mock_budget_key(
        budget_key_name, Uuid::GenerateUuid(), async_executor, journal_service,
        nosql_database_provider, mock_metric_client, mock_config_provider);
    EXPECT_EQ(mock_budget_key.OnJournalServiceRecoverCallback(
                  journal_log_context.request->data, kDefaultUuid),
              SuccessExecutionResult());

    // Call the callback to get the time of buffer timeframe manager.
    budget_key.on_log_load_budget_key_callback =
        [&](AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&,
            Uuid& timeframe_manager_id,
            AsyncContext<JournalLogRequest, JournalLogResponse>&) {
          EXPECT_EQ(timeframe_manager_id,
                    mock_budget_key.GetBudgetKeyTimeframeManagerId());
        };

    journal_log_context.result = SuccessExecutionResult();
    journal_log_context.callback(journal_log_context);
    return SuccessExecutionResult();
  };

  AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>
      load_budget_key_context;
  load_budget_key_context.callback =
      [&](AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
              load_budget_key_context) { EXPECT_EQ(true, false); };

  EXPECT_EQ(budget_key.LoadBudgetKey(load_budget_key_context),
            SuccessExecutionResult());
}

TEST(BudgetKeyTest, SerializeBudgetKey) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  shared_ptr<BudgetKeyTimeframeManagerInterface> budget_key_manager = nullptr;
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  mock_config_provider->Set(kBudgetKeyTableName, string("PBS_BudgetKeys"));
  MockBudgetKey budget_key(
      budget_key_name, Uuid::GenerateUuid(), async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  Uuid timeframe_manager_id = Uuid::GenerateUuid();
  auto bytes_buffer = make_shared<BytesBuffer>();
  EXPECT_EQ(budget_key.SerializeBudgetKey(timeframe_manager_id, *bytes_buffer),
            SuccessExecutionResult());

  EXPECT_EQ(
      budget_key.OnJournalServiceRecoverCallback(bytes_buffer, kDefaultUuid),
      SuccessExecutionResult());

  EXPECT_EQ(budget_key.GetBudgetKeyTimeframeManagerId(), timeframe_manager_id);
}

TEST(BudgetKeyTest, OnLogLoadBudgetKeyCallback) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  shared_ptr<BudgetKeyTimeframeManagerInterface> budget_key_manager = nullptr;
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  MockBudgetKey budget_key(
      budget_key_name, Uuid::GenerateUuid(), async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>
      load_budget_key_context;
  Uuid budget_key_timeframe_manager_id;
  core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>
      journal_log_context;

  load_budget_key_context.callback =
      [](AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
             load_budget_key_context) {
        EXPECT_THAT(load_budget_key_context.result,
                    ResultIs(FailureExecutionResult(123)));
      };

  journal_log_context.result = FailureExecutionResult(123);
  budget_key.OnLogLoadBudgetKeyCallback(load_budget_key_context,
                                        budget_key_timeframe_manager_id,
                                        journal_log_context);

  load_budget_key_context.callback =
      [](AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
             load_budget_key_context) {
        EXPECT_THAT(load_budget_key_context.result,
                    ResultIs(RetryExecutionResult(123)));
      };

  journal_log_context.result = RetryExecutionResult(123);
  budget_key.OnLogLoadBudgetKeyCallback(load_budget_key_context,
                                        budget_key_timeframe_manager_id,
                                        journal_log_context);
}

TEST(BudgetKeyTest, OnLogLoadBudgetKeyCallbackWithFailure) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  shared_ptr<BudgetKeyTimeframeManagerInterface> budget_key_manager = nullptr;
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  MockBudgetKey budget_key(
      budget_key_name, Uuid::GenerateUuid(), async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>
      load_budget_key_context;
  Uuid budget_key_timeframe_manager_id = Uuid::GenerateUuid();
  core::AsyncContext<core::JournalLogRequest, core::JournalLogResponse>
      journal_log_context;

  load_budget_key_context.callback =
      [](AsyncContext<LoadBudgetKeyRequest, LoadBudgetKeyResponse>&
             load_budget_key_context) {
        EXPECT_SUCCESS(load_budget_key_context.result);
      };

  journal_log_context.result = SuccessExecutionResult();

  EXPECT_EQ(budget_key.GetBudgetConsumptionTransactionProtocol(), nullptr);

  budget_key.OnLogLoadBudgetKeyCallback(load_budget_key_context,
                                        budget_key_timeframe_manager_id,
                                        journal_log_context);

  EXPECT_EQ(budget_key.GetBudgetKeyTimeframeManagerId(),
            budget_key_timeframe_manager_id);
}

TEST(BudgetKeyTest, OnJournalServiceRecoverCallbackInvalidLog) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  shared_ptr<BudgetKeyTimeframeManagerInterface> budget_key_manager = nullptr;
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  MockBudgetKey budget_key(
      budget_key_name, Uuid::GenerateUuid(), async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  auto bytes_buffer = make_shared<BytesBuffer>();
  EXPECT_EQ(
      budget_key.OnJournalServiceRecoverCallback(bytes_buffer, kDefaultUuid),
      FailureExecutionResult(
          core::errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED));
  EXPECT_EQ(budget_key.GetBudgetConsumptionTransactionProtocol(), nullptr);
}

TEST(BudgetKeyTest, OnJournalServiceRecoverCallbackInvalidVersion) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  shared_ptr<BudgetKeyTimeframeManagerInterface> budget_key_manager = nullptr;
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  MockBudgetKey budget_key(
      budget_key_name, Uuid::GenerateUuid(), async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  BudgetKeyLog budget_key_log;
  budget_key_log.mutable_version()->set_major(10);
  budget_key_log.mutable_version()->set_minor(11);

  size_t bytes_serialized = 0;
  auto bytes_buffer = make_shared<BytesBuffer>(budget_key_log.ByteSizeLong());
  EXPECT_EQ(Serialization::SerializeProtoMessage<BudgetKeyLog>(
                *bytes_buffer, 0, budget_key_log, bytes_serialized),
            SuccessExecutionResult());
  EXPECT_EQ(budget_key_log.ByteSizeLong(), bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(
      budget_key.OnJournalServiceRecoverCallback(bytes_buffer, kDefaultUuid),
      FailureExecutionResult(
          core::errors::SC_SERIALIZATION_VERSION_IS_INVALID));
  EXPECT_EQ(budget_key.GetBudgetConsumptionTransactionProtocol(), nullptr);
}

TEST(BudgetKeyTest, OnJournalServiceRecoverCallbackInvalidLog1_0) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  shared_ptr<BudgetKeyTimeframeManagerInterface> budget_key_manager = nullptr;
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  MockBudgetKey budget_key(
      budget_key_name, Uuid::GenerateUuid(), async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  BudgetKeyLog budget_key_log;
  budget_key_log.mutable_version()->set_major(1);
  budget_key_log.mutable_version()->set_minor(0);

  BytesBuffer log_body_1_0(1);
  budget_key_log.set_log_body(log_body_1_0.bytes->data(), log_body_1_0.length);

  size_t bytes_serialized = 0;
  auto bytes_buffer = make_shared<BytesBuffer>(budget_key_log.ByteSizeLong());
  EXPECT_EQ(Serialization::SerializeProtoMessage<BudgetKeyLog>(
                *bytes_buffer, 0, budget_key_log, bytes_serialized),
            SuccessExecutionResult());
  EXPECT_EQ(budget_key_log.ByteSizeLong(), bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(
      budget_key.OnJournalServiceRecoverCallback(bytes_buffer, kDefaultUuid),
      FailureExecutionResult(
          core::errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED));
  EXPECT_EQ(budget_key.GetBudgetConsumptionTransactionProtocol(), nullptr);
}

TEST(BudgetKeyTest, OnJournalServiceRecoverCallbackValidLog) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider;
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  mock_config_provider->Set(kBudgetKeyTableName, string("PBS_BudgetKeys"));
  auto mock_journal_service = make_shared<MockJournalServiceWithOverrides>(
      bucket_name, partition_name, async_executor, blob_storage_provider,
      mock_metric_client, mock_config_provider);
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKey budget_key(
      budget_key_name, Uuid::GenerateUuid(), async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  BudgetKeyLog budget_key_log;
  budget_key_log.mutable_version()->set_major(1);
  budget_key_log.mutable_version()->set_minor(0);

  BudgetKeyLog_1_0 budget_key_log_1_0;
  budget_key_log_1_0.mutable_timeframe_manager_id()->set_high(123);
  budget_key_log_1_0.mutable_timeframe_manager_id()->set_low(456);

  size_t bytes_serialized = 0;
  BytesBuffer log_body_1_0_buffer(budget_key_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BudgetKeyLog_1_0>(
      log_body_1_0_buffer, 0, budget_key_log_1_0, bytes_serialized);
  log_body_1_0_buffer.length = bytes_serialized;

  budget_key_log.set_log_body(log_body_1_0_buffer.bytes->data(),
                              log_body_1_0_buffer.length);

  bytes_serialized = 0;
  auto bytes_buffer = make_shared<BytesBuffer>(budget_key_log.ByteSizeLong());
  EXPECT_EQ(Serialization::SerializeProtoMessage<BudgetKeyLog>(
                *bytes_buffer, 0, budget_key_log, bytes_serialized),
            SuccessExecutionResult());
  EXPECT_EQ(budget_key_log.ByteSizeLong(), bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  OnLogRecoveredCallback callback;

  EXPECT_EQ(
      budget_key.OnJournalServiceRecoverCallback(bytes_buffer, kDefaultUuid),
      SuccessExecutionResult());

  EXPECT_EQ(mock_journal_service->GetSubscribersMap().Find(
                budget_key.GetBudgetKeyTimeframeManagerId(), callback),
            SuccessExecutionResult());

  EXPECT_EQ(budget_key.GetBudgetKeyTimeframeManagerId().high, 123);
  EXPECT_EQ(budget_key.GetBudgetKeyTimeframeManagerId().low, 456);
  EXPECT_NE(budget_key.GetBudgetConsumptionTransactionProtocol(), nullptr);
}

TEST(BudgetKeyTest, CheckpointNoTimeframeManager) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  shared_ptr<BudgetKeyTimeframeManagerInterface> budget_key_manager = nullptr;
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  BudgetKey budget_key(budget_key_name, Uuid::GenerateUuid(), async_executor,
                       journal_service, nosql_database_provider, nullptr,
                       budget_key_transaction_protocol, mock_metric_client,
                       mock_config_provider, mock_aggregate_metric);
  auto logs = make_shared<list<CheckpointLog>>();
  EXPECT_SUCCESS(budget_key.Checkpoint(logs));
  EXPECT_EQ(logs->size(), 1);
}

TEST(BudgetKeyTest, CanUnload) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto mock_budget_key_timeframe_manager =
      make_shared<MockBudgetKeyTimeframeManager>();
  shared_ptr<BudgetKeyTimeframeManagerInterface> budget_key_manager =
      mock_budget_key_timeframe_manager;
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  mock_config_provider->Set(kBudgetKeyTableName, string("PBS_BudgetKeys"));
  BudgetKey budget_key(budget_key_name, Uuid::GenerateUuid(), async_executor,
                       journal_service, nosql_database_provider,
                       budget_key_manager, budget_key_transaction_protocol,
                       mock_metric_client, mock_config_provider,
                       mock_aggregate_metric);

  mock_budget_key_timeframe_manager->can_unload_mock = []() {
    return FailureExecutionResult(123);
  };

  EXPECT_THAT(budget_key.CanUnload(), ResultIs(FailureExecutionResult(123)));
}

TEST(BudgetKeyTest, Checkpoint) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<BudgetKeyTimeframeManagerInterface> budget_key_manager =
      make_shared<MockBudgetKeyTimeframeManager>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  mock_config_provider->Set(kBudgetKeyTableName, string("PBS_BudgetKeys"));
  BudgetKey budget_key(budget_key_name, Uuid::GenerateUuid(), async_executor,
                       journal_service, nosql_database_provider,
                       budget_key_manager, budget_key_transaction_protocol,
                       mock_metric_client, mock_config_provider,
                       mock_aggregate_metric);

  auto logs = make_shared<list<CheckpointLog>>();
  EXPECT_SUCCESS(budget_key.Checkpoint(logs));
  EXPECT_EQ(logs->size(), 1);

  auto it = logs->begin();
  EXPECT_EQ(it->component_id, budget_key.GetId());
  EXPECT_NE(it->log_id.low, 0);
  EXPECT_NE(it->log_id.high, 0);
  EXPECT_EQ(it->log_status, JournalLogStatus::Log);

  MockBudgetKey recovery_budget_key(
      budget_key_name, budget_key.GetId(), async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  auto bytes_buffer = make_shared<BytesBuffer>(it->bytes_buffer);
  EXPECT_EQ(recovery_budget_key.OnJournalServiceRecoverCallback(bytes_buffer,
                                                                kDefaultUuid),
            SuccessExecutionResult());

  Uuid timeframe_manager_id;
  timeframe_manager_id.high = ~budget_key.GetId().high;
  timeframe_manager_id.low = ~budget_key.GetId().low;
  EXPECT_EQ(recovery_budget_key.GetBudgetKeyTimeframeManagerId(),
            timeframe_manager_id);
}

TEST(BudgetKeyTest, CheckpointFailureWithTimeframeManager) {
  auto budget_key_name = make_shared<string>("test_budget_key");
  auto budget_key_transaction_protocol =
      make_shared<MockConsumeBudgetTransactionProtocol>();

  auto mock_journal_service = make_shared<MockJournalService>();
  shared_ptr<JournalServiceInterface> journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto mock_budget_key_manager = make_shared<MockBudgetKeyTimeframeManager>();
  auto budget_key_manager =
      static_pointer_cast<BudgetKeyTimeframeManagerInterface>(
          mock_budget_key_manager);
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  BudgetKey budget_key(budget_key_name, Uuid::GenerateUuid(), async_executor,
                       journal_service, nosql_database_provider,
                       budget_key_manager, budget_key_transaction_protocol,
                       mock_metric_client, mock_config_provider,
                       mock_aggregate_metric);

  mock_budget_key_manager->checkpoint_mock = [](auto&) {
    return FailureExecutionResult(1234);
  };

  auto logs = make_shared<list<CheckpointLog>>();
  EXPECT_THAT(budget_key.Checkpoint(logs),
              ResultIs(FailureExecutionResult(1234)));
  EXPECT_EQ(logs->size(), 1);
}
}  // namespace google::scp::pbs::test
