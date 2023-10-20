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

#include "pbs/budget_key_timeframe_manager/src/budget_key_timeframe_manager.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <list>
#include <utility>
#include <variant>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/serialization/src/error_codes.h"
#include "core/common/serialization/src/serialization.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/async_context.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/journal_service/mock/mock_journal_service.h"
#include "core/journal_service/mock/mock_journal_service_with_overrides.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider_no_overrides.h"
#include "core/test/utils/conditional_wait.h"
#include "pbs/budget_key/src/budget_key.h"
#include "pbs/budget_key_timeframe_manager/mock/mock_budget_key_timeframe_manager_with_override.h"
#include "pbs/budget_key_timeframe_manager/src/budget_key_timeframe_serialization.h"
#include "pbs/budget_key_timeframe_manager/src/budget_key_timeframe_utils.h"
#include "pbs/budget_key_timeframe_manager/src/error_codes.h"
#include "pbs/budget_key_timeframe_manager/src/proto/budget_key_timeframe_manager.pb.h"
#include "pbs/interface/configuration_keys.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::BlobStorageProviderInterface;
using google::scp::core::BytesBuffer;
using google::scp::core::CheckpointLog;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetDatabaseItemRequest;
using google::scp::core::GetDatabaseItemResponse;
using google::scp::core::JournalLogRequest;
using google::scp::core::JournalLogResponse;
using google::scp::core::JournalLogStatus;
using google::scp::core::JournalServiceInterface;
using google::scp::core::NoSQLDatabaseAttributeName;
using google::scp::core::NoSqlDatabaseKeyValuePair;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::NoSQLDatabaseValidAttributeValueTypes;
using google::scp::core::OnLogRecoveredCallback;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::UpsertDatabaseItemRequest;
using google::scp::core::UpsertDatabaseItemResponse;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::Serialization;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::journal_service::mock::MockJournalService;
using google::scp::core::journal_service::mock::MockJournalServiceWithOverrides;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProvider;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProviderNoOverrides;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::MockAggregateMetric;
using google::scp::cpio::MockMetricClient;
using google::scp::pbs::BudgetKey;
using google::scp::pbs::BudgetKeyTimeframeManager;
using google::scp::pbs::budget_key_timeframe_manager::Utils;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BatchBudgetKeyTimeframeLog_1_0;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BudgetKeyTimeframeGroupLog_1_0;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BudgetKeyTimeframeLog_1_0;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BudgetKeyTimeframeManagerLog;
using google::scp::pbs::budget_key_timeframe_manager::proto::
    BudgetKeyTimeframeManagerLog_1_0;
using google::scp::pbs::budget_key_timeframe_manager::proto::OperationType;
using google::scp::pbs::buget_key_timeframe_manager::mock::
    MockBudgetKeyTimeframeManager;
using std::atomic;
using std::function;
using std::get;
using std::list;
using std::make_pair;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::vector;
using std::chrono::hours;
using std::chrono::minutes;
using std::chrono::nanoseconds;
using std::chrono::seconds;

static constexpr Uuid kDefaultUuid = {0, 0};

static shared_ptr<MockAggregateMetric> mock_aggregate_metric =
    make_shared<MockAggregateMetric>();

namespace google::scp::pbs::test {
TEST(BudgetKeyTimeframeManagerTest, InitShouldSubscribe) {
  auto budget_key_name = make_shared<string>("budget_key_name");
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider;
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();

  auto mock_journal_service = make_shared<MockJournalServiceWithOverrides>(
      bucket_name, partition_name, async_executor, blob_storage_provider,
      mock_metric_client, mock_config_provider);
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  mock_config_provider->Set(kBudgetKeyTableName, string("PBS_BudgetKeys"));

  BudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider,
      mock_aggregate_metric);

  OnLogRecoveredCallback callback;
  EXPECT_EQ(mock_journal_service->GetSubscribersMap().Find(id, callback),
            FailureExecutionResult(
                core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));

  budget_key_timeframe_manager.Init();

  EXPECT_EQ(mock_journal_service->GetSubscribersMap().Find(id, callback),
            SuccessExecutionResult());
}

TEST(BudgetKeyTimeframeManagerTest, LoadWithEmptyReportingTimesIsDisallowed) {
  auto budget_key_name = make_shared<string>("budget_key_name");
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  Uuid id = Uuid::GenerateUuid();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context;
  load_budget_key_timeframe_context.request =
      make_shared<LoadBudgetKeyTimeframeRequest>();
  load_budget_key_timeframe_context.request->reporting_times = {};
  auto execution_result =
      budget_key_timeframe_manager.Load(load_budget_key_timeframe_context);
  EXPECT_EQ(execution_result,
            FailureExecutionResult(
                core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_EMPTY_REQUEST));
}

TEST(BudgetKeyTimeframeManagerTest,
     LoadWithMultipleReportingTimesOfSameTimeBucketIsDisallowed) {
  auto budget_key_name = make_shared<string>("budget_key_name");
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  Uuid id = Uuid::GenerateUuid();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context;
  load_budget_key_timeframe_context.request =
      make_shared<LoadBudgetKeyTimeframeRequest>();
  load_budget_key_timeframe_context.request->reporting_times = {1, 2, 3, 4};
  auto execution_result =
      budget_key_timeframe_manager.Load(load_budget_key_timeframe_context);
  EXPECT_EQ(
      execution_result,
      FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_REPEATED_TIMEBUCKETS));
}

TEST(BudgetKeyTimeframeManagerTest,
     LoadWithMultipleReportingTimesOfDifferentTimeGroupsIsDisallowed) {
  auto budget_key_name = make_shared<string>("budget_key_name");
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  Uuid id = Uuid::GenerateUuid();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context;
  load_budget_key_timeframe_context.request =
      make_shared<LoadBudgetKeyTimeframeRequest>();
  load_budget_key_timeframe_context.request->reporting_times = {
      1, 2, 3, 4, (nanoseconds(4) + hours(24)).count()};
  auto execution_result =
      budget_key_timeframe_manager.Load(load_budget_key_timeframe_context);
  EXPECT_EQ(execution_result,
            FailureExecutionResult(
                core::errors::
                    SC_BUDGET_KEY_TIMEFRAME_MANAGER_MULTIPLE_TIMEFRAME_GROUPS));
}

TEST(BudgetKeyTimeframeManagerTest, UpdateWithEmptyTimeframesIsDisallowed) {
  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();

  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  EXPECT_EQ(
      budget_key_timeframe_manager.Update(update_budget_key_timeframe_context),
      FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_EMPTY_REQUEST));
}

TEST(BudgetKeyTimeframeManagerTest,
     UpdateWithMultipleTimeframesOfSameTimeBucketIsDisallowed) {
  Timestamp reporting_time1 = nanoseconds(1000).count();
  Timestamp reporting_time2 = (nanoseconds(1000) + minutes(56)).count();

  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = reporting_time1;

  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = reporting_time2;

  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  EXPECT_EQ(
      budget_key_timeframe_manager.Update(update_budget_key_timeframe_context),
      FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_REPEATED_TIMEBUCKETS));
}

TEST(BudgetKeyTimeframeManagerTest,
     UpdateWithMultipleTimeframesOfDifferentTimeGroupsIsDisallowed) {
  Timestamp reporting_time1 = nanoseconds(1000).count();
  Timestamp reporting_time2 = (nanoseconds(1000) + hours(25)).count();

  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = reporting_time1;

  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = reporting_time2;

  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  EXPECT_EQ(
      budget_key_timeframe_manager.Update(update_budget_key_timeframe_context),
      FailureExecutionResult(
          core::errors::
              SC_BUDGET_KEY_TIMEFRAME_MANAGER_MULTIPLE_TIMEFRAME_GROUPS));
}

TEST(BudgetKeyTimeframeManagerTest, LoadKey) {
  auto budget_key_name = make_shared<string>("budget_key_name");
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  Timestamp reporting_time = 20;
  atomic<bool> condition(false);
  Uuid id = Uuid::GenerateUuid();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  LoadBudgetKeyTimeframeRequest load_budget_key_request{
      .reporting_times = {reporting_time}};

  budget_key_timeframe_manager.load_timeframe_group_from_db_mock =
      [&](AsyncContext<LoadBudgetKeyTimeframeRequest,
                       LoadBudgetKeyTimeframeResponse>&,
          shared_ptr<BudgetKeyTimeframeGroup>&) {
        condition = true;
        return SuccessExecutionResult();
      };

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(
              move(load_budget_key_request)),
          [](auto& load_budget_key_timeframe_context) {
            EXPECT_EQ(load_budget_key_timeframe_context.result,
                      SuccessExecutionResult());
            return load_budget_key_timeframe_context.result;
          });
  auto result =
      budget_key_timeframe_manager.Load(load_budget_key_timeframe_context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return condition.load(); });
}

TEST(BudgetKeyTimeframeManagerTest, RetryUntilLoaded) {
  auto budget_key_name = make_shared<string>("budget_key_name");
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();

  Timestamp reporting_time = 20;
  atomic<bool> condition(false);
  Uuid id = Uuid::GenerateUuid();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  LoadBudgetKeyTimeframeRequest load_budget_key_request{
      .reporting_times = {reporting_time}};

  budget_key_timeframe_manager.load_timeframe_group_from_db_mock =
      [&](AsyncContext<LoadBudgetKeyTimeframeRequest,
                       LoadBudgetKeyTimeframeResponse>& context,
          shared_ptr<BudgetKeyTimeframeGroup>&) {
        context.Finish();
        condition = true;
        return SuccessExecutionResult();
      };

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(
              move(load_budget_key_request)),
          [&budget_key_timeframe_manager,
           &reporting_time](auto& load_budget_key_timeframe_context) mutable {
            auto time_group = Utils::GetTimeGroup(reporting_time);
            shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
            budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                time_group, budget_key_timeframe_group);
            budget_key_timeframe_group->is_loaded = false;
            EXPECT_EQ(
                budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                    ->IsEvictable(time_group),
                false);
            return load_budget_key_timeframe_context.result;
          });

  // Load the first time
  budget_key_timeframe_manager.Load(load_budget_key_timeframe_context);
  WaitUntil([&]() { return condition.load(); });
  // Load the second time
  auto result =
      budget_key_timeframe_manager.Load(load_budget_key_timeframe_context);
  EXPECT_EQ(
      result,
      RetryExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_ENTRY_IS_LOADING));
  WaitUntil([&]() { return condition.load(); });
}

TEST(BudgetKeyTimeframeManagerTest, RetryUntilLoadedAfterDeletion) {
  auto budget_key_name = make_shared<string>("budget_key_name");
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  Timestamp reporting_time = 20;
  atomic<bool> condition = false;
  Uuid id = Uuid::GenerateUuid();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  LoadBudgetKeyTimeframeRequest load_budget_key_request{
      .reporting_times = {reporting_time}};

  budget_key_timeframe_manager.load_timeframe_group_from_db_mock =
      [&](AsyncContext<LoadBudgetKeyTimeframeRequest,
                       LoadBudgetKeyTimeframeResponse>& context,
          shared_ptr<BudgetKeyTimeframeGroup>&) {
        context.Finish();
        condition = true;
        return SuccessExecutionResult();
      };

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(
              move(load_budget_key_request)),
          [&budget_key_timeframe_manager,
           &reporting_time](auto& load_budget_key_timeframe_context) mutable {
            auto time_group = Utils::GetTimeGroup(reporting_time);
            shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
            budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                time_group, budget_key_timeframe_group);
            budget_key_timeframe_group->is_loaded = false;
            budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                ->MarkAsBeingDeleted(time_group);
            budget_key_timeframe_group->is_loaded = false;
            return load_budget_key_timeframe_context.result;
          });

  // Load the first time
  budget_key_timeframe_manager.Load(load_budget_key_timeframe_context);
  WaitUntil([&]() { return condition.load(); });

  // Load the second time
  auto result =
      budget_key_timeframe_manager.Load(load_budget_key_timeframe_context);
  EXPECT_EQ(
      result,
      RetryExecutionResult(
          core::errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED));
}

TEST(BudgetKeyTimeframeManagerTest, BecomeTheLoaderIfLoadingFails) {
  auto budget_key_name = make_shared<string>("budget_key_name");
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  Timestamp reporting_time = 20;
  atomic<bool> condition = false;
  Uuid id = Uuid::GenerateUuid();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  LoadBudgetKeyTimeframeRequest load_budget_key_request{
      .reporting_times = {reporting_time}};

  budget_key_timeframe_manager.load_timeframe_group_from_db_mock =
      [&](AsyncContext<LoadBudgetKeyTimeframeRequest,
                       LoadBudgetKeyTimeframeResponse>&
              load_budget_key_timeframe_context,
          shared_ptr<BudgetKeyTimeframeGroup>&) {
        condition = true;
        return SuccessExecutionResult();
      };

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(
              move(load_budget_key_request)),
          [](auto& load_budget_key_timeframe_context) mutable {});

  // Load the first time
  budget_key_timeframe_manager.Load(load_budget_key_timeframe_context);
  WaitUntil([&]() { return condition.load(); });
  condition = false;
  // Load the second time
  auto result =
      budget_key_timeframe_manager.Load(load_budget_key_timeframe_context);
  EXPECT_EQ(
      result,
      RetryExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_ENTRY_IS_LOADING));

  auto time_group = Utils::GetTimeGroup(reporting_time);
  shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
      time_group, budget_key_timeframe_group);
  budget_key_timeframe_group->needs_loader = true;
  result = budget_key_timeframe_manager.Load(load_budget_key_timeframe_context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(budget_key_timeframe_group->needs_loader.load(), false);
}

TEST(BudgetKeyTimeframeManagerTest, DoNotLoadIfKeyExists) {
  auto budget_key_name = make_shared<string>("budget_key_name");
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  Timestamp reporting_time = nanoseconds(20).count();
  atomic<bool> condition(false);
  Uuid id = Uuid::GenerateUuid();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  LoadBudgetKeyTimeframeRequest load_budget_key_request{
      .reporting_times = {reporting_time}};

  budget_key_timeframe_manager.load_timeframe_group_from_db_mock =
      [&](AsyncContext<LoadBudgetKeyTimeframeRequest,
                       LoadBudgetKeyTimeframeResponse>&
              load_budget_key_timeframe_context,
          shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group) {
        auto time_bucket = Utils::GetTimeBucket(
            load_budget_key_timeframe_context.request->reporting_times[0]);
        auto budget_key_timeframe =
            make_shared<BudgetKeyTimeframe>(time_bucket);
        auto pair = make_pair(time_bucket, budget_key_timeframe);
        if (budget_key_timeframe_group->budget_key_timeframes
                .Insert(pair, budget_key_timeframe)
                .Successful()) {
          budget_key_timeframe->token_count = kMaxToken;
        }

        condition = true;
        return SuccessExecutionResult();
      };

  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context(
          make_shared<LoadBudgetKeyTimeframeRequest>(
              move(load_budget_key_request)),
          [](auto& load_budget_key_timeframe_context) {});

  // Load the first time
  EXPECT_EQ(
      budget_key_timeframe_manager.Load(load_budget_key_timeframe_context),
      SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  condition = false;

  auto time_group = Utils::GetTimeGroup(reporting_time);
  shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
      time_group, budget_key_timeframe_group);
  budget_key_timeframe_group->is_loaded = true;

  shared_ptr<BudgetKeyTimeframe> budget_key_timeframe;
  auto time_bucket = Utils::GetTimeBucket(reporting_time);
  EXPECT_EQ(budget_key_timeframe_group->budget_key_timeframes.Find(
                time_bucket, budget_key_timeframe),
            SuccessExecutionResult());
  EXPECT_EQ(budget_key_timeframe->token_count, kMaxToken);

  // Load the second time
  load_budget_key_timeframe_context.response = nullptr;
  load_budget_key_timeframe_context.result = FailureExecutionResult(SC_UNKNOWN);
  auto result =
      budget_key_timeframe_manager.Load(load_budget_key_timeframe_context);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(condition.load(), false);
}

TEST(BudgetKeyTimeframeManagerTest, DoNotLoadIfKeysOfSameTimegroupExist) {
  auto budget_key_name = make_shared<string>("budget_key_name");
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();

  Timestamp reporting_time1 = nanoseconds(20).count();
  Timestamp reporting_time2 = (nanoseconds(20) + hours(2)).count();

  TokenCount timeframe_1_tokens = 10;
  TokenCount timeframe_2_tokens = 20;

  atomic<bool> condition(false);
  Uuid id = Uuid::GenerateUuid();

  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  budget_key_timeframe_manager.load_timeframe_group_from_db_mock =
      [&](AsyncContext<LoadBudgetKeyTimeframeRequest,
                       LoadBudgetKeyTimeframeResponse>&
              load_budget_key_timeframe_context,
          shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group) {
        // Load timeframe 1 as per request
        {
          auto time_bucket = Utils::GetTimeBucket(
              load_budget_key_timeframe_context.request->reporting_times[0]);
          auto budget_key_timeframe =
              make_shared<BudgetKeyTimeframe>(time_bucket);
          auto pair = make_pair(time_bucket, budget_key_timeframe);
          if (budget_key_timeframe_group->budget_key_timeframes.Insert(
                  pair, budget_key_timeframe) == SuccessExecutionResult()) {
            budget_key_timeframe->token_count = timeframe_1_tokens;
          }
        }

        // But also load timeframe 2 (since this timeframe belongs to the same
        // time group). Ideally, in the real implementation, all the timeframes
        // belonging to the time group will be loaded here.
        {
          auto time_bucket = Utils::GetTimeBucket(reporting_time2);
          auto budget_key_timeframe =
              make_shared<BudgetKeyTimeframe>(time_bucket);
          auto pair = make_pair(time_bucket, budget_key_timeframe);
          if (budget_key_timeframe_group->budget_key_timeframes.Insert(
                  pair, budget_key_timeframe) == SuccessExecutionResult()) {
            budget_key_timeframe->token_count = timeframe_2_tokens;
          }
        }

        condition = true;
        return SuccessExecutionResult();
      };

  // Load for the first timeframe should load from the database
  {
    LoadBudgetKeyTimeframeRequest load_budget_key_request{
        .reporting_times = {reporting_time1}};
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
        load_budget_key_timeframe_context(
            make_shared<LoadBudgetKeyTimeframeRequest>(
                move(load_budget_key_request)),
            [](auto& load_budget_key_timeframe_context) {});

    EXPECT_EQ(
        budget_key_timeframe_manager.Load(load_budget_key_timeframe_context),
        SuccessExecutionResult());
    WaitUntil([&]() { return condition.load(); });
    condition = false;
  }

  // Verify that the timeframes have been loaded.
  auto time_group = Utils::GetTimeGroup(reporting_time1);
  shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
      time_group, budget_key_timeframe_group);
  budget_key_timeframe_group->is_loaded = true;

  {
    shared_ptr<BudgetKeyTimeframe> budget_key_timeframe;
    auto time_bucket = Utils::GetTimeBucket(reporting_time1);
    EXPECT_EQ(budget_key_timeframe_group->budget_key_timeframes.Find(
                  time_bucket, budget_key_timeframe),
              SuccessExecutionResult());
    EXPECT_EQ(budget_key_timeframe->token_count, timeframe_1_tokens);
  }

  {
    shared_ptr<BudgetKeyTimeframe> budget_key_timeframe;
    auto time_bucket = Utils::GetTimeBucket(reporting_time2);
    EXPECT_EQ(budget_key_timeframe_group->budget_key_timeframes.Find(
                  time_bucket, budget_key_timeframe),
              SuccessExecutionResult());
    EXPECT_EQ(budget_key_timeframe->token_count, timeframe_2_tokens);
  }

  // Load for the second timeframe should not load again
  {
    atomic<bool> request_completed = false;
    LoadBudgetKeyTimeframeRequest load_budget_key_request{
        .reporting_times = {reporting_time2}};
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
        load_budget_key_timeframe_context(
            make_shared<LoadBudgetKeyTimeframeRequest>(
                move(load_budget_key_request)),
            [&request_completed](auto& load_budget_key_timeframe_context) {
              request_completed = true;
            });

    EXPECT_EQ(
        budget_key_timeframe_manager.Load(load_budget_key_timeframe_context),
        SuccessExecutionResult());
    WaitUntil([&]() { return request_completed.load(); });
    EXPECT_EQ(condition.load(), false);
    EXPECT_EQ(
        load_budget_key_timeframe_context.response->budget_key_frames.size(),
        1);
    EXPECT_EQ(load_budget_key_timeframe_context.response->budget_key_frames[0]
                  ->time_bucket_index,
              Utils::GetTimeBucket(reporting_time2));
    EXPECT_EQ(load_budget_key_timeframe_context.response->budget_key_frames[0]
                  ->token_count,
              timeframe_2_tokens);
  }

  // Load both timeframes should not load again
  {
    atomic<bool> request_completed = false;
    LoadBudgetKeyTimeframeRequest load_budget_key_request{
        .reporting_times = {reporting_time1, reporting_time2}};
    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
        load_budget_key_timeframe_context(
            make_shared<LoadBudgetKeyTimeframeRequest>(
                move(load_budget_key_request)),
            [&request_completed](auto& load_budget_key_timeframe_context) {
              request_completed = true;
            });

    EXPECT_EQ(
        budget_key_timeframe_manager.Load(load_budget_key_timeframe_context),
        SuccessExecutionResult());
    WaitUntil([&]() { return request_completed.load(); });
    EXPECT_EQ(condition.load(), false);
    EXPECT_EQ(
        load_budget_key_timeframe_context.response->budget_key_frames.size(),
        2);
    EXPECT_EQ(load_budget_key_timeframe_context.response->budget_key_frames[0]
                  ->time_bucket_index,
              Utils::GetTimeBucket(reporting_time1));
    EXPECT_EQ(load_budget_key_timeframe_context.response->budget_key_frames[0]
                  ->token_count,
              timeframe_1_tokens);
    EXPECT_EQ(load_budget_key_timeframe_context.response->budget_key_frames[1]
                  ->time_bucket_index,
              Utils::GetTimeBucket(reporting_time2));
    EXPECT_EQ(load_budget_key_timeframe_context.response->budget_key_frames[1]
                  ->token_count,
              timeframe_2_tokens);
  }
}

TEST(BudgetKeyTimeframeManagerTest, UpdateLogWithSingleTimeframe) {
  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_token_count = 10;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = 1000;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .token_count = 23;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.low = 123;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.high = 456;
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  mock_journal_service->log_mock =
      [&](AsyncContext<JournalLogRequest, JournalLogResponse>&
              journal_log_context) {
        EXPECT_EQ(journal_log_context.request->component_id.high,
                  budget_key_timeframe_manager.GetId().high);
        EXPECT_EQ(journal_log_context.request->component_id.low,
                  budget_key_timeframe_manager.GetId().low);
        EXPECT_EQ(journal_log_context.request->log_status,
                  JournalLogStatus::Log);
        EXPECT_NE(journal_log_context.request->data->bytes->size(), 0);
        EXPECT_NE(journal_log_context.request->data->length, 0);
        EXPECT_NE(journal_log_context.request->data->capacity, 0);

        // Verify that the log type is for single time frame update
        BudgetKeyTimeframeManagerLog budget_key_time_frame_manager_log;
        auto execution_result = budget_key_timeframe_manager::Serialization::
            DeserializeBudgetKeyTimeframeManagerLog(
                *(journal_log_context.request->data),
                budget_key_time_frame_manager_log);
        if (execution_result != SuccessExecutionResult()) {
          return execution_result;
        }
        BudgetKeyTimeframeManagerLog_1_0 budget_key_time_frame_manager_log_1_0;
        execution_result = budget_key_timeframe_manager::Serialization::
            DeserializeBudgetKeyTimeframeManagerLog_1_0(
                budget_key_time_frame_manager_log.log_body(),
                budget_key_time_frame_manager_log_1_0);
        if (execution_result != SuccessExecutionResult()) {
          return execution_result;
        }
        EXPECT_EQ(budget_key_time_frame_manager_log_1_0.operation_type(),
                  OperationType::UPDATE_TIMEFRAME_RECORD);

        // Use journal service callback apply to verify correctness of log data
        EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                      journal_log_context.request->data, kDefaultUuid),
                  SuccessExecutionResult());
        return SuccessExecutionResult();
      };

  EXPECT_EQ(
      budget_key_timeframe_manager.Update(update_budget_key_timeframe_context),
      FailureExecutionResult(
          core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));

  shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
  Timestamp reporting_time = 1000;
  auto time_group = Utils::GetTimeGroup(reporting_time);
  auto budget_key_timeframe_group_pair =
      make_pair(time_group, make_shared<BudgetKeyTimeframeGroup>(time_group));
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      budget_key_timeframe_group_pair, budget_key_timeframe_group);

  auto time_bucket = Utils::GetTimeBucket(reporting_time);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(time_bucket);
  auto budget_key_timeframe_pair = make_pair(time_bucket, budget_key_timeframe);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      budget_key_timeframe_pair, budget_key_timeframe);

  budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
      ->DisableEviction(time_group);
  EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                ->IsEvictable(time_group),
            false);

  EXPECT_EQ(
      budget_key_timeframe_manager.Update(update_budget_key_timeframe_context),
      SuccessExecutionResult());

  EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                ->IsEvictable(time_group),
            false);
}

TEST(BudgetKeyTimeframeManagerTest, UpdateLogWithMultipleTimeframes) {
  Timestamp reporting_time1 = nanoseconds(1000).count();
  Timestamp reporting_time2 = (nanoseconds(1000) + hours(2)).count();

  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_token_count = 10;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = reporting_time1;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .token_count = 23;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.low = 123;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.high = 456;

  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_token_count = 20;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = reporting_time2;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .token_count = 46;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.low = 456;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.high = 789;

  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  mock_journal_service->log_mock =
      [&](AsyncContext<JournalLogRequest, JournalLogResponse>&
              journal_log_context) {
        EXPECT_EQ(journal_log_context.request->component_id.high,
                  budget_key_timeframe_manager.GetId().high);
        EXPECT_EQ(journal_log_context.request->component_id.low,
                  budget_key_timeframe_manager.GetId().low);
        EXPECT_EQ(journal_log_context.request->log_status,
                  JournalLogStatus::Log);
        EXPECT_NE(journal_log_context.request->data->bytes->size(), 0);
        EXPECT_NE(journal_log_context.request->data->length, 0);
        EXPECT_NE(journal_log_context.request->data->capacity, 0);

        // Verify that the log type is for batch time frame update
        BudgetKeyTimeframeManagerLog budget_key_time_frame_manager_log;
        auto execution_result = budget_key_timeframe_manager::Serialization::
            DeserializeBudgetKeyTimeframeManagerLog(
                *(journal_log_context.request->data),
                budget_key_time_frame_manager_log);
        if (execution_result != SuccessExecutionResult()) {
          return execution_result;
        }
        BudgetKeyTimeframeManagerLog_1_0 budget_key_time_frame_manager_log_1_0;
        execution_result = budget_key_timeframe_manager::Serialization::
            DeserializeBudgetKeyTimeframeManagerLog_1_0(
                budget_key_time_frame_manager_log.log_body(),
                budget_key_time_frame_manager_log_1_0);
        if (execution_result != SuccessExecutionResult()) {
          return execution_result;
        }
        EXPECT_EQ(budget_key_time_frame_manager_log_1_0.operation_type(),
                  OperationType::BATCH_UPDATE_TIMEFRAME_RECORDS_OF_TIMEGROUP);

        // Use journal service callback apply to verify correctness of log data
        EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                      journal_log_context.request->data, kDefaultUuid),
                  SuccessExecutionResult());
        return SuccessExecutionResult();
      };

  EXPECT_EQ(
      budget_key_timeframe_manager.Update(update_budget_key_timeframe_context),
      FailureExecutionResult(
          core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));

  shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
  auto time_group = Utils::GetTimeGroup(reporting_time1);
  auto budget_key_timeframe_group_pair =
      make_pair(time_group, make_shared<BudgetKeyTimeframeGroup>(time_group));
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      budget_key_timeframe_group_pair, budget_key_timeframe_group);

  {
    auto time_bucket = Utils::GetTimeBucket(reporting_time1);
    auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(time_bucket);
    auto budget_key_timeframe_pair =
        make_pair(time_bucket, budget_key_timeframe);
    budget_key_timeframe_group->budget_key_timeframes.Insert(
        budget_key_timeframe_pair, budget_key_timeframe);
  }

  {
    auto time_bucket = Utils::GetTimeBucket(reporting_time2);
    auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(time_bucket);
    auto budget_key_timeframe_pair =
        make_pair(time_bucket, budget_key_timeframe);
    budget_key_timeframe_group->budget_key_timeframes.Insert(
        budget_key_timeframe_pair, budget_key_timeframe);
  }

  budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
      ->DisableEviction(time_group);
  EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                ->IsEvictable(time_group),
            false);

  EXPECT_EQ(
      budget_key_timeframe_manager.Update(update_budget_key_timeframe_context),
      SuccessExecutionResult());

  EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                ->IsEvictable(time_group),
            false);
}

TEST(BudgetKeyTimeframeManagerTest, OnLogUpdateCallbackFailure) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  Timestamp reporting_time = 10;
  auto time_group = Utils::GetTimeGroup(reporting_time);
  auto budget_key_timeframe_group =
      make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto budget_key_timeframe_group_pair =
      make_pair(time_group, budget_key_timeframe_group);
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      budget_key_timeframe_group_pair, budget_key_timeframe_group);

  auto time_bucket = Utils::GetTimeBucket(reporting_time);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(time_bucket);
  auto budget_key_timeframe_pair = make_pair(time_bucket, budget_key_timeframe);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      budget_key_timeframe_pair, budget_key_timeframe);

  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = 10;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .token_count = 100;
  update_budget_key_timeframe_context.callback =
      [](AsyncContext<UpdateBudgetKeyTimeframeRequest,
                      UpdateBudgetKeyTimeframeResponse>&
             update_budget_key_timeframe_context) {
        EXPECT_EQ(update_budget_key_timeframe_context.result,
                  FailureExecutionResult(123));
      };
  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.result = FailureExecutionResult(123);

  budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
      ->DisableEviction(time_group);

  EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                ->IsEvictable(time_group),
            false);

  std::vector<shared_ptr<BudgetKeyTimeframe>> budget_key_timeframes = {
      budget_key_timeframe};
  budget_key_timeframe_manager.OnLogUpdateCallback(
      update_budget_key_timeframe_context, budget_key_timeframes,
      journal_log_context);

  EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                ->IsEvictable(time_group),
            true);

  // In-memory timeframe is untouched
  EXPECT_NE(
      budget_key_timeframe->token_count,
      update_budget_key_timeframe_context.request->timeframes_to_update.back()
          .token_count);
}

TEST(BudgetKeyTimeframeManagerTest, OnLogUpdateCallbackRetry) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  Timestamp reporting_time = 10;
  auto time_group = Utils::GetTimeGroup(reporting_time);
  auto budget_key_timeframe_group =
      make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto budget_key_timeframe_group_pair =
      make_pair(time_group, budget_key_timeframe_group);
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      budget_key_timeframe_group_pair, budget_key_timeframe_group);

  auto time_bucket = Utils::GetTimeBucket(reporting_time);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(time_bucket);
  auto budget_key_timeframe_pair = make_pair(time_bucket, budget_key_timeframe);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      budget_key_timeframe_pair, budget_key_timeframe);

  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = 10;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .token_count = 100;
  update_budget_key_timeframe_context.callback =
      [](AsyncContext<UpdateBudgetKeyTimeframeRequest,
                      UpdateBudgetKeyTimeframeResponse>&
             update_budget_key_timeframe_context) {
        EXPECT_EQ(update_budget_key_timeframe_context.result,
                  RetryExecutionResult(123));
      };
  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.result = RetryExecutionResult(123);
  budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
      ->DisableEviction(time_group);

  EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                ->IsEvictable(time_group),
            false);

  std::vector<shared_ptr<BudgetKeyTimeframe>> budget_key_timeframes = {
      budget_key_timeframe};
  budget_key_timeframe_manager.OnLogUpdateCallback(
      update_budget_key_timeframe_context, budget_key_timeframes,
      journal_log_context);

  EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                ->IsEvictable(time_group),
            true);

  // In-memory timeframe is untouched
  EXPECT_NE(
      budget_key_timeframe->token_count,
      update_budget_key_timeframe_context.request->timeframes_to_update.back()
          .token_count);
}

TEST(BudgetKeyTimeframeManagerTest, OnLogUpdateCallbackSuccessNoEntry) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = 10;
  update_budget_key_timeframe_context.callback =
      [](AsyncContext<UpdateBudgetKeyTimeframeRequest,
                      UpdateBudgetKeyTimeframeResponse>&
             update_budget_key_timeframe_context) {
        EXPECT_EQ(update_budget_key_timeframe_context.result,
                  SuccessExecutionResult());
      };

  Timestamp reporting_time = 10;
  auto time_group = Utils::GetTimeGroup(reporting_time);
  auto budget_key_timeframe_group =
      make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto budget_key_timeframe_group_pair =
      make_pair(time_group, budget_key_timeframe_group);
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      budget_key_timeframe_group_pair, budget_key_timeframe_group);

  auto time_bucket = Utils::GetTimeBucket(reporting_time);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(time_bucket);
  auto budget_key_timeframe_pair = make_pair(time_bucket, budget_key_timeframe);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      budget_key_timeframe_pair, budget_key_timeframe);

  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.result = SuccessExecutionResult();

  budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
      ->DisableEviction(time_group);

  EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                ->IsEvictable(time_group),
            false);

  std::vector<shared_ptr<BudgetKeyTimeframe>> budget_key_timeframes = {
      budget_key_timeframe};
  budget_key_timeframe_manager.OnLogUpdateCallback(
      update_budget_key_timeframe_context, budget_key_timeframes,
      journal_log_context);

  EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                ->IsEvictable(time_group),
            true);
}

TEST(BudgetKeyTimeframeManagerTest, OnLogUpdateCallbackSuccessWithEntry) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  Timestamp reporting_time = 10;
  auto time_group = Utils::GetTimeGroup(reporting_time);
  auto budget_key_timeframe_group =
      make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto budget_key_timeframe_group_pair =
      make_pair(time_group, budget_key_timeframe_group);
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      budget_key_timeframe_group_pair, budget_key_timeframe_group);

  auto time_bucket = Utils::GetTimeBucket(reporting_time);
  auto budget_key_timeframe = make_shared<BudgetKeyTimeframe>(time_bucket);
  budget_key_timeframe->active_token_count = 1;
  budget_key_timeframe->token_count = 23;
  budget_key_timeframe->active_transaction_id = Uuid::GenerateUuid();
  auto budget_key_timeframe_pair = make_pair(time_bucket, budget_key_timeframe);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      budget_key_timeframe_pair, budget_key_timeframe);

  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = 10;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_token_count = 20;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .token_count = 3;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.low = 123;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.high = 456;
  update_budget_key_timeframe_context.callback =
      [&](AsyncContext<UpdateBudgetKeyTimeframeRequest,
                       UpdateBudgetKeyTimeframeResponse>&
              update_budget_key_timeframe_context) {
        EXPECT_EQ(update_budget_key_timeframe_context.result,
                  SuccessExecutionResult());
        EXPECT_EQ(budget_key_timeframe->active_token_count, 20);
        EXPECT_EQ(budget_key_timeframe->time_bucket_index, time_bucket);
        EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().low, 123);
        EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().high, 456);
        EXPECT_EQ(budget_key_timeframe->token_count, 3);
      };
  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.result = SuccessExecutionResult();

  std::vector<shared_ptr<BudgetKeyTimeframe>> budget_key_timeframes = {
      budget_key_timeframe};
  budget_key_timeframe_manager.OnLogUpdateCallback(
      update_budget_key_timeframe_context, budget_key_timeframes,
      journal_log_context);
}

TEST(BudgetKeyTimeframeManagerTest,
     OnLogUpdateCallbackSuccessWithMultipleEntries) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  // Both belong to same time group
  Timestamp reporting_time1 = nanoseconds(10).count();
  Timestamp reporting_time2 = (nanoseconds(10) + hours(2)).count();
  TimeGroup time_group = Utils::GetTimeGroup(reporting_time1);

  auto budget_key_timeframe_group =
      make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto budget_key_timeframe_group_pair =
      make_pair(time_group, budget_key_timeframe_group);
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      budget_key_timeframe_group_pair, budget_key_timeframe_group);

  auto time_bucket1 = Utils::GetTimeBucket(reporting_time1);
  auto budget_key_timeframe1 = make_shared<BudgetKeyTimeframe>(time_bucket1);
  budget_key_timeframe1->active_token_count = 1;
  budget_key_timeframe1->token_count = 23;
  budget_key_timeframe1->active_transaction_id = Uuid::GenerateUuid();
  auto budget_key_timeframe_pair1 =
      make_pair(time_bucket1, budget_key_timeframe1);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      budget_key_timeframe_pair1, budget_key_timeframe1);

  auto time_bucket2 = Utils::GetTimeBucket(reporting_time2);
  auto budget_key_timeframe2 = make_shared<BudgetKeyTimeframe>(time_bucket2);
  budget_key_timeframe2->active_token_count = 1;
  budget_key_timeframe2->token_count = 23;
  budget_key_timeframe2->active_transaction_id = Uuid::GenerateUuid();
  auto budget_key_timeframe_pair2 =
      make_pair(time_bucket2, budget_key_timeframe2);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      budget_key_timeframe_pair2, budget_key_timeframe2);

  AsyncContext<UpdateBudgetKeyTimeframeRequest,
               UpdateBudgetKeyTimeframeResponse>
      update_budget_key_timeframe_context;
  update_budget_key_timeframe_context.request =
      make_shared<UpdateBudgetKeyTimeframeRequest>();
  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = reporting_time1;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_token_count = 20;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .token_count = 10;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.low = 123;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.high = 456;

  update_budget_key_timeframe_context.request->timeframes_to_update
      .emplace_back();
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .reporting_time = reporting_time2;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_token_count = 50;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .token_count = 30;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.low = 111;
  update_budget_key_timeframe_context.request->timeframes_to_update.back()
      .active_transaction_id.high = 222;

  update_budget_key_timeframe_context.callback =
      [&](AsyncContext<UpdateBudgetKeyTimeframeRequest,
                       UpdateBudgetKeyTimeframeResponse>&
              update_budget_key_timeframe_context) {
        EXPECT_EQ(update_budget_key_timeframe_context.result,
                  SuccessExecutionResult());

        EXPECT_EQ(budget_key_timeframe1->active_token_count, 20);
        EXPECT_EQ(budget_key_timeframe1->time_bucket_index, time_bucket1);
        EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load().low, 123);
        EXPECT_EQ(budget_key_timeframe1->active_transaction_id.load().high,
                  456);
        EXPECT_EQ(budget_key_timeframe1->token_count, 10);

        EXPECT_EQ(budget_key_timeframe2->active_token_count, 50);
        EXPECT_EQ(budget_key_timeframe2->time_bucket_index, time_bucket2);
        EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load().low, 111);
        EXPECT_EQ(budget_key_timeframe2->active_transaction_id.load().high,
                  222);
        EXPECT_EQ(budget_key_timeframe2->token_count, 30);
      };
  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.result = SuccessExecutionResult();

  std::vector<shared_ptr<BudgetKeyTimeframe>> budget_key_timeframes = {
      budget_key_timeframe1, budget_key_timeframe2};
  budget_key_timeframe_manager.OnLogUpdateCallback(
      update_budget_key_timeframe_context, budget_key_timeframes,
      journal_log_context);
}

TEST(BudgetKeyTimeframeManagerTest, OnJournalServiceRecoverCallbackInvalidLog) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  auto bytes_buffer = make_shared<BytesBuffer>(1);
  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED));
}

TEST(BudgetKeyTimeframeManagerTest,
     OnJournalServiceRecoverCallbackInvalidLogVersion) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
  budget_key_timeframe_manager_log.mutable_version()->set_major(10);
  budget_key_timeframe_manager_log.mutable_version()->set_minor(2);
  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_timeframe_manager_log.ByteSizeLong());
  size_t bytes_serialized = 0;
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog>(
      *bytes_buffer, 0, budget_key_timeframe_manager_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;
  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_SERIALIZATION_VERSION_IS_INVALID));
}

TEST(BudgetKeyTimeframeManagerTest,
     OnJournalServiceRecoverCallbackInvalidLog1_0) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
  budget_key_timeframe_manager_log.mutable_version()->set_major(1);
  budget_key_timeframe_manager_log.mutable_version()->set_minor(0);
  BytesBuffer log_body(10);
  budget_key_timeframe_manager_log.set_log_body(log_body.bytes->data(),
                                                log_body.capacity);
  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_timeframe_manager_log.ByteSizeLong());
  size_t bytes_serialized = 0;
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog>(
      *bytes_buffer, 0, budget_key_timeframe_manager_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;
  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED));
}

TEST(BudgetKeyTimeframeManagerTest,
     OnJournalServiceRecoverCallbackInsertTimeframeGroupWithEmptyBody1_0) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  auto time_group = 10;

  BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
  budget_key_timeframe_manager_log.mutable_version()->set_major(1);
  budget_key_timeframe_manager_log.mutable_version()->set_minor(0);

  BudgetKeyTimeframeManagerLog_1_0 budget_key_timeframe_manager_log_1_0;
  budget_key_timeframe_manager_log_1_0.set_time_group(time_group);
  budget_key_timeframe_manager_log_1_0.set_operation_type(
      OperationType::INSERT_TIMEGROUP_INTO_CACHE);

  size_t bytes_serialized = 0;
  BytesBuffer log_body_bytes_buffer(
      budget_key_timeframe_manager_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog_1_0>(
      log_body_bytes_buffer, 0, budget_key_timeframe_manager_log_1_0,
      bytes_serialized);
  log_body_bytes_buffer.length = bytes_serialized;

  budget_key_timeframe_manager_log.set_log_body(
      log_body_bytes_buffer.bytes->data(), log_body_bytes_buffer.length);

  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_timeframe_manager_log.ByteSizeLong());
  bytes_serialized = 0;
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog>(
      *bytes_buffer, 0, budget_key_timeframe_manager_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::
                    SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA));
}

TEST(BudgetKeyTimeframeManagerTest,
     OnJournalServiceRecoverCallbackInsertTimeframeGroupWithNonEmptyBody1_0) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  auto time_group = 10;

  BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
  budget_key_timeframe_manager_log.mutable_version()->set_major(1);
  budget_key_timeframe_manager_log.mutable_version()->set_minor(0);

  BudgetKeyTimeframeManagerLog_1_0 budget_key_timeframe_manager_log_1_0;
  budget_key_timeframe_manager_log_1_0.set_time_group(time_group);
  budget_key_timeframe_manager_log_1_0.set_operation_type(
      OperationType::INSERT_TIMEGROUP_INTO_CACHE);

  BudgetKeyTimeframeGroupLog_1_0 budget_key_timeframe_group_log_1_0;
  budget_key_timeframe_group_log_1_0.set_time_group(time_group);
  auto item = budget_key_timeframe_group_log_1_0.add_items();
  item->set_time_bucket(10);
  item->set_token_count(12);

  size_t bytes_serialized = 0;
  BytesBuffer budget_key_timeframe_group_log_1_0_bytes_buffer(
      budget_key_timeframe_group_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeGroupLog_1_0>(
      budget_key_timeframe_group_log_1_0_bytes_buffer, 0,
      budget_key_timeframe_group_log_1_0, bytes_serialized);
  budget_key_timeframe_group_log_1_0_bytes_buffer.length = bytes_serialized;

  budget_key_timeframe_manager_log_1_0.set_log_body(
      budget_key_timeframe_group_log_1_0_bytes_buffer.bytes->data(),
      budget_key_timeframe_group_log_1_0_bytes_buffer.length);

  bytes_serialized = 0;
  BytesBuffer log_body_bytes_buffer(
      budget_key_timeframe_manager_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog_1_0>(
      log_body_bytes_buffer, 0, budget_key_timeframe_manager_log_1_0,
      bytes_serialized);
  log_body_bytes_buffer.length = bytes_serialized;

  budget_key_timeframe_manager_log.set_log_body(
      log_body_bytes_buffer.bytes->data(), log_body_bytes_buffer.length);

  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_timeframe_manager_log.ByteSizeLong());
  bytes_serialized = 0;
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog>(
      *bytes_buffer, 0, budget_key_timeframe_manager_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            SuccessExecutionResult());

  shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
  EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                time_group, budget_key_timeframe_group),
            SuccessExecutionResult());

  EXPECT_EQ(budget_key_timeframe_group->is_loaded.load(), true);
  EXPECT_EQ(budget_key_timeframe_group->needs_loader.load(), false);
  EXPECT_EQ(budget_key_timeframe_group->time_group, time_group);

  shared_ptr<BudgetKeyTimeframe> budget_key_timeframe;
  EXPECT_EQ(budget_key_timeframe_group->budget_key_timeframes.Find(
                10, budget_key_timeframe),
            SuccessExecutionResult());
  EXPECT_EQ(budget_key_timeframe->token_count.load(), 12);
  EXPECT_EQ(budget_key_timeframe->active_token_count.load(), 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().high, 0);
  EXPECT_EQ(budget_key_timeframe->active_transaction_id.load().low, 0);
}

TEST(BudgetKeyTimeframeManagerTest,
     OnJournalServiceRecoverCallbackValidGroupLogRemoveTimeframeGroup1_0) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  auto time_group = 1234;
  auto budget_key_timeframe_group =
      make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto budget_key_timeframe_group_pair =
      make_pair(time_group, budget_key_timeframe_group);
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      budget_key_timeframe_group_pair, budget_key_timeframe_group);

  EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                time_group, budget_key_timeframe_group),
            SuccessExecutionResult());

  BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
  budget_key_timeframe_manager_log.mutable_version()->set_major(1);
  budget_key_timeframe_manager_log.mutable_version()->set_minor(0);

  BudgetKeyTimeframeManagerLog_1_0 budget_key_timeframe_manager_log_1_0;
  budget_key_timeframe_manager_log_1_0.set_time_group(time_group);
  budget_key_timeframe_manager_log_1_0.set_operation_type(
      OperationType::REMOVE_TIMEGROUP_FROM_CACHE);

  size_t bytes_serialized = 0;
  BytesBuffer log_body_bytes_buffer(
      budget_key_timeframe_manager_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog_1_0>(
      log_body_bytes_buffer, 0, budget_key_timeframe_manager_log_1_0,
      bytes_serialized);
  log_body_bytes_buffer.length = bytes_serialized;

  budget_key_timeframe_manager_log.set_log_body(
      log_body_bytes_buffer.bytes->data(), log_body_bytes_buffer.length);

  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_timeframe_manager_log.ByteSizeLong());
  bytes_serialized = 0;
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog>(
      *bytes_buffer, 0, budget_key_timeframe_manager_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            SuccessExecutionResult());

  EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                time_group, budget_key_timeframe_group),
            FailureExecutionResult(
                core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));

  // if it is called again, no actions need to be taken.
  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            SuccessExecutionResult());
}

TEST(BudgetKeyTimeframeManagerTest,
     OnJournalServiceRecoverCallbackValidGroupLogUpdateTimeframe1_0) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  TimeGroup time_group = 1234;
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
  budget_key_timeframe_manager_log.mutable_version()->set_major(1);
  budget_key_timeframe_manager_log.mutable_version()->set_minor(0);

  BudgetKeyTimeframeManagerLog_1_0 budget_key_timeframe_manager_log_1_0;
  budget_key_timeframe_manager_log_1_0.set_time_group(time_group);
  budget_key_timeframe_manager_log_1_0.set_operation_type(
      OperationType::UPDATE_TIMEFRAME_RECORD);

  BudgetKeyTimeframeLog_1_0 budget_key_timeframe_log_1_0;
  budget_key_timeframe_log_1_0.set_time_bucket(1);
  budget_key_timeframe_log_1_0.set_token_count(5);
  budget_key_timeframe_log_1_0.set_active_token_count(3);
  budget_key_timeframe_log_1_0.mutable_active_transaction_id()->set_high(123);
  budget_key_timeframe_log_1_0.mutable_active_transaction_id()->set_low(456);

  size_t bytes_serialized = 0;
  BytesBuffer budget_key_timeframe_log_1_0_buffer(
      budget_key_timeframe_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeLog_1_0>(
      budget_key_timeframe_log_1_0_buffer, 0, budget_key_timeframe_log_1_0,
      bytes_serialized);
  budget_key_timeframe_log_1_0_buffer.length = bytes_serialized;

  budget_key_timeframe_manager_log_1_0.set_log_body(
      budget_key_timeframe_log_1_0_buffer.bytes->data(),
      budget_key_timeframe_log_1_0_buffer.length);

  bytes_serialized = 0;
  BytesBuffer budget_key_timeframe_manager_log_1_0_buffer(
      budget_key_timeframe_manager_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog_1_0>(
      budget_key_timeframe_manager_log_1_0_buffer, 0,
      budget_key_timeframe_manager_log_1_0, bytes_serialized);
  budget_key_timeframe_manager_log_1_0_buffer.length = bytes_serialized;

  budget_key_timeframe_manager_log.set_log_body(
      budget_key_timeframe_manager_log_1_0_buffer.bytes->data(),
      budget_key_timeframe_manager_log_1_0_buffer.length);

  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_timeframe_manager_log.ByteSizeLong());
  bytes_serialized = 0;
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog>(
      *bytes_buffer, 0, budget_key_timeframe_manager_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));

  auto timeframe_group = make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto timeframe_group_pair = make_pair(time_group, timeframe_group);
  EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
                timeframe_group_pair, timeframe_group),
            SuccessExecutionResult());

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            SuccessExecutionResult());

  auto timeframe = make_shared<BudgetKeyTimeframe>(1);
  EXPECT_EQ(timeframe_group->budget_key_timeframes.Find(1, timeframe),
            SuccessExecutionResult());

  EXPECT_EQ(timeframe->time_bucket_index, 1);
  EXPECT_EQ(timeframe->active_token_count, 3);
  EXPECT_EQ(timeframe->token_count, 5);
  EXPECT_EQ(timeframe->active_transaction_id.load().high, 123);
  EXPECT_EQ(timeframe->active_transaction_id.load().low, 456);
  EXPECT_EQ(timeframe_group->is_loaded.load(), false);
}

TEST(BudgetKeyTimeframeManagerTest,
     OnJournalServiceRecoverCallbackInvalidEmptyLog1_0) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  TimeGroup time_group = 1234;
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
  budget_key_timeframe_manager_log.mutable_version()->set_major(1);
  budget_key_timeframe_manager_log.mutable_version()->set_minor(0);

  BudgetKeyTimeframeManagerLog_1_0 budget_key_timeframe_manager_log_1_0;
  budget_key_timeframe_manager_log_1_0.set_time_group(time_group);
  budget_key_timeframe_manager_log_1_0.set_operation_type(
      OperationType::UPDATE_TIMEFRAME_RECORD);

  size_t bytes_serialized = 0;
  BytesBuffer budget_key_timeframe_manager_log_1_0_buffer(
      budget_key_timeframe_manager_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog_1_0>(
      budget_key_timeframe_manager_log_1_0_buffer, 0,
      budget_key_timeframe_manager_log_1_0, bytes_serialized);
  budget_key_timeframe_manager_log_1_0_buffer.length = bytes_serialized;

  budget_key_timeframe_manager_log.set_log_body(
      budget_key_timeframe_manager_log_1_0_buffer.bytes->data(),
      budget_key_timeframe_manager_log_1_0_buffer.length);

  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_timeframe_manager_log.ByteSizeLong());
  bytes_serialized = 0;
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog>(
      *bytes_buffer, 0, budget_key_timeframe_manager_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));

  auto timeframe_group = make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto timeframe_group_pair = make_pair(time_group, timeframe_group);
  EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
                timeframe_group_pair, timeframe_group),
            SuccessExecutionResult());

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED));

  auto timeframe = make_shared<BudgetKeyTimeframe>(1);
  EXPECT_EQ(timeframe_group->budget_key_timeframes.Find(1, timeframe),
            FailureExecutionResult(
                core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));
}

TEST(BudgetKeyTimeframeManagerTest,
     OnJournalServiceRecoverCallbackBatchUpdateTimeframeWithEmptyBody1_0) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  auto time_group = 10;

  BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
  budget_key_timeframe_manager_log.mutable_version()->set_major(1);
  budget_key_timeframe_manager_log.mutable_version()->set_minor(0);

  BudgetKeyTimeframeManagerLog_1_0 budget_key_timeframe_manager_log_1_0;
  budget_key_timeframe_manager_log_1_0.set_time_group(time_group);
  budget_key_timeframe_manager_log_1_0.set_operation_type(
      OperationType::BATCH_UPDATE_TIMEFRAME_RECORDS_OF_TIMEGROUP);

  size_t bytes_serialized = 0;
  BytesBuffer log_body_bytes_buffer(
      budget_key_timeframe_manager_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog_1_0>(
      log_body_bytes_buffer, 0, budget_key_timeframe_manager_log_1_0,
      bytes_serialized);
  log_body_bytes_buffer.length = bytes_serialized;

  budget_key_timeframe_manager_log.set_log_body(
      log_body_bytes_buffer.bytes->data(), log_body_bytes_buffer.length);

  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_timeframe_manager_log.ByteSizeLong());
  bytes_serialized = 0;
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog>(
      *bytes_buffer, 0, budget_key_timeframe_manager_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));

  auto timeframe_group = make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto timeframe_group_pair = make_pair(time_group, timeframe_group);
  EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
                timeframe_group_pair, timeframe_group),
            SuccessExecutionResult());

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_INVALID_LOG));
}

TEST(BudgetKeyTimeframeManagerTest,
     OnJournalServiceRecoverCallbackBatchUpdateTimeframeWithInvalidBody1_0) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  auto time_group = 10;

  BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
  budget_key_timeframe_manager_log.mutable_version()->set_major(1);
  budget_key_timeframe_manager_log.mutable_version()->set_minor(0);

  BudgetKeyTimeframeManagerLog_1_0 budget_key_timeframe_manager_log_1_0;
  budget_key_timeframe_manager_log_1_0.set_time_group(time_group);
  budget_key_timeframe_manager_log_1_0.set_operation_type(
      OperationType::BATCH_UPDATE_TIMEFRAME_RECORDS_OF_TIMEGROUP);

  // Populating a budget key time frame instead of a batch budget key time frame
  BudgetKeyTimeframeLog_1_0 budget_key_timeframe_log_1_0;
  budget_key_timeframe_log_1_0.set_time_bucket(1);
  budget_key_timeframe_log_1_0.set_token_count(5);
  budget_key_timeframe_log_1_0.set_active_token_count(3);
  budget_key_timeframe_log_1_0.mutable_active_transaction_id()->set_high(123);
  budget_key_timeframe_log_1_0.mutable_active_transaction_id()->set_low(456);

  size_t bytes_serialized = 0;
  BytesBuffer budget_key_timeframe_log_1_0_buffer(
      budget_key_timeframe_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeLog_1_0>(
      budget_key_timeframe_log_1_0_buffer, 0, budget_key_timeframe_log_1_0,
      bytes_serialized);
  budget_key_timeframe_log_1_0_buffer.length = bytes_serialized;

  budget_key_timeframe_manager_log_1_0.set_log_body(
      budget_key_timeframe_log_1_0_buffer.bytes->data(),
      budget_key_timeframe_log_1_0_buffer.length);

  bytes_serialized = 0;
  BytesBuffer log_body_bytes_buffer(
      budget_key_timeframe_manager_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog_1_0>(
      log_body_bytes_buffer, 0, budget_key_timeframe_manager_log_1_0,
      bytes_serialized);
  log_body_bytes_buffer.length = bytes_serialized;

  budget_key_timeframe_manager_log.set_log_body(
      log_body_bytes_buffer.bytes->data(), log_body_bytes_buffer.length);

  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_timeframe_manager_log.ByteSizeLong());
  bytes_serialized = 0;
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog>(
      *bytes_buffer, 0, budget_key_timeframe_manager_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));

  auto timeframe_group = make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto timeframe_group_pair = make_pair(time_group, timeframe_group);
  EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
                timeframe_group_pair, timeframe_group),
            SuccessExecutionResult());

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_INVALID_LOG));
}

TEST(BudgetKeyTimeframeManagerTest,
     OnJournalServiceRecoverCallbackBatchUpdateTimeframeWithValidBody1_0) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  TimeGroup time_group = 1234;
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);
  BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
  budget_key_timeframe_manager_log.mutable_version()->set_major(1);
  budget_key_timeframe_manager_log.mutable_version()->set_minor(0);

  BudgetKeyTimeframeManagerLog_1_0 budget_key_timeframe_manager_log_1_0;
  budget_key_timeframe_manager_log_1_0.set_time_group(time_group);
  budget_key_timeframe_manager_log_1_0.set_operation_type(
      OperationType::BATCH_UPDATE_TIMEFRAME_RECORDS_OF_TIMEGROUP);

  BatchBudgetKeyTimeframeLog_1_0 batch_budget_key_timeframe_log_1_0;
  {
    auto budget_key_timeframe_log_1_0 =
        batch_budget_key_timeframe_log_1_0.add_items();
    budget_key_timeframe_log_1_0->set_time_bucket(1);
    budget_key_timeframe_log_1_0->set_token_count(5);
    budget_key_timeframe_log_1_0->set_active_token_count(3);
    budget_key_timeframe_log_1_0->mutable_active_transaction_id()->set_high(
        123);
    budget_key_timeframe_log_1_0->mutable_active_transaction_id()->set_low(456);
  }
  {
    auto budget_key_timeframe_log_1_0 =
        batch_budget_key_timeframe_log_1_0.add_items();
    budget_key_timeframe_log_1_0->set_time_bucket(2);
    budget_key_timeframe_log_1_0->set_token_count(10);
    budget_key_timeframe_log_1_0->set_active_token_count(6);
    budget_key_timeframe_log_1_0->mutable_active_transaction_id()->set_high(
        1234);
    budget_key_timeframe_log_1_0->mutable_active_transaction_id()->set_low(
        4567);
  }
  {
    auto budget_key_timeframe_log_1_0 =
        batch_budget_key_timeframe_log_1_0.add_items();
    budget_key_timeframe_log_1_0->set_time_bucket(3);
    budget_key_timeframe_log_1_0->set_token_count(15);
    budget_key_timeframe_log_1_0->set_active_token_count(9);
    budget_key_timeframe_log_1_0->mutable_active_transaction_id()->set_high(
        12345);
    budget_key_timeframe_log_1_0->mutable_active_transaction_id()->set_low(
        45678);
  }

  size_t bytes_serialized = 0;
  BytesBuffer batch_budget_key_timeframe_log_1_0_buffer(
      batch_budget_key_timeframe_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BatchBudgetKeyTimeframeLog_1_0>(
      batch_budget_key_timeframe_log_1_0_buffer, 0,
      batch_budget_key_timeframe_log_1_0, bytes_serialized);
  batch_budget_key_timeframe_log_1_0_buffer.length = bytes_serialized;

  budget_key_timeframe_manager_log_1_0.set_log_body(
      batch_budget_key_timeframe_log_1_0_buffer.bytes->data(),
      batch_budget_key_timeframe_log_1_0_buffer.length);

  bytes_serialized = 0;
  BytesBuffer budget_key_timeframe_manager_log_1_0_buffer(
      budget_key_timeframe_manager_log_1_0.ByteSizeLong());
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog_1_0>(
      budget_key_timeframe_manager_log_1_0_buffer, 0,
      budget_key_timeframe_manager_log_1_0, bytes_serialized);
  budget_key_timeframe_manager_log_1_0_buffer.length = bytes_serialized;

  budget_key_timeframe_manager_log.set_log_body(
      budget_key_timeframe_manager_log_1_0_buffer.bytes->data(),
      budget_key_timeframe_manager_log_1_0_buffer.length);

  auto bytes_buffer =
      make_shared<BytesBuffer>(budget_key_timeframe_manager_log.ByteSizeLong());
  bytes_serialized = 0;
  Serialization::SerializeProtoMessage<BudgetKeyTimeframeManagerLog>(
      *bytes_buffer, 0, budget_key_timeframe_manager_log, bytes_serialized);
  bytes_buffer->length = bytes_serialized;

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            FailureExecutionResult(
                core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));

  auto timeframe_group = make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto timeframe_group_pair = make_pair(time_group, timeframe_group);
  EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
                timeframe_group_pair, timeframe_group),
            SuccessExecutionResult());

  EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                bytes_buffer, kDefaultUuid),
            SuccessExecutionResult());
  {
    auto timeframe = make_shared<BudgetKeyTimeframe>(1);
    EXPECT_EQ(timeframe_group->budget_key_timeframes.Find(1, timeframe),
              SuccessExecutionResult());
    EXPECT_EQ(timeframe->time_bucket_index, 1);
    EXPECT_EQ(timeframe->active_token_count, 3);
    EXPECT_EQ(timeframe->token_count, 5);
    EXPECT_EQ(timeframe->active_transaction_id.load().high, 123);
    EXPECT_EQ(timeframe->active_transaction_id.load().low, 456);
    EXPECT_EQ(timeframe_group->is_loaded.load(), false);
  }
  {
    auto timeframe = make_shared<BudgetKeyTimeframe>(2);
    EXPECT_EQ(timeframe_group->budget_key_timeframes.Find(2, timeframe),
              SuccessExecutionResult());
    EXPECT_EQ(timeframe->time_bucket_index, 2);
    EXPECT_EQ(timeframe->active_token_count, 6);
    EXPECT_EQ(timeframe->token_count, 10);
    EXPECT_EQ(timeframe->active_transaction_id.load().high, 1234);
    EXPECT_EQ(timeframe->active_transaction_id.load().low, 4567);
    EXPECT_EQ(timeframe_group->is_loaded.load(), false);
  }
  {
    auto timeframe = make_shared<BudgetKeyTimeframe>(3);
    EXPECT_EQ(timeframe_group->budget_key_timeframes.Find(3, timeframe),
              SuccessExecutionResult());
    EXPECT_EQ(timeframe->time_bucket_index, 3);
    EXPECT_EQ(timeframe->active_token_count, 9);
    EXPECT_EQ(timeframe->token_count, 15);
    EXPECT_EQ(timeframe->active_transaction_id.load().high, 12345);
    EXPECT_EQ(timeframe->active_transaction_id.load().low, 45678);
    EXPECT_EQ(timeframe_group->is_loaded.load(), false);
  }

  vector<TimeBucket> keys;
  EXPECT_EQ(timeframe_group->budget_key_timeframes.Keys(keys),
            SuccessExecutionResult());
  EXPECT_EQ(keys.size(), 3);
}

TEST(BudgetKeyTimeframeManagerTest, LoadTimeframeGroupFromDBResults) {
  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context;
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  mock_config_provider->Set(kBudgetKeyTableName, string("PBS_BudgetKeys"));
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<NoSQLDatabaseProviderInterface>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  budget_key_timeframe_manager.Init();

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  TimeBucket reporting_time = 1660498765350482296;
  TimeGroup time_group = Utils::GetTimeGroup(reporting_time);
  shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group =
      make_shared<BudgetKeyTimeframeGroup>(time_group);

  for (auto result : results) {
    EXPECT_CALL(*mock_nosql_database_provider, GetDatabaseItem)
        .WillOnce(
            [&](AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
                    get_database_item_context) {
              if (result.Successful()) {
                EXPECT_EQ(*get_database_item_context.request->table_name,
                          "PBS_BudgetKeys");
                EXPECT_EQ(*get_database_item_context.request->partition_key
                               ->attribute_name,
                          "Budget_Key");
                EXPECT_EQ(get<string>(*get_database_item_context.request
                                           ->partition_key->attribute_value),
                          "budget_key_name");
                EXPECT_EQ(*get_database_item_context.request->sort_key
                               ->attribute_name,
                          "Timeframe");
                EXPECT_EQ(get<string>(*get_database_item_context.request
                                           ->sort_key->attribute_value),
                          "19218");
              }

              return result;
            });

    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
        load_budget_key_timeframe_context;
    load_budget_key_timeframe_context.request =
        make_shared<LoadBudgetKeyTimeframeRequest>();
    load_budget_key_timeframe_context.request->reporting_times = {
        reporting_time};
    EXPECT_EQ(
        budget_key_timeframe_manager.LoadTimeframeGroupFromDB(
            load_budget_key_timeframe_context, budget_key_timeframe_group),
        result);
  }
}

TEST(BudgetKeyTimeframeManagerTest,
     OnLoadTimeframeGroupFromDBCallbackInvalidResults) {
  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context;

  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<NoSQLDatabaseProviderInterface>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  vector<ExecutionResult> results = {
      FailureExecutionResult(123), RetryExecutionResult(1234),
      FailureExecutionResult(
          core::errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND)};

  for (auto result : results) {
    atomic<bool> condition(false);
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
        get_database_item_context;
    get_database_item_context.result = result;

    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
        load_budget_key_timeframe_context;
    load_budget_key_timeframe_context.request =
        make_shared<LoadBudgetKeyTimeframeRequest>();
    TimeBucket reporting_time = 1660498765350482296;
    auto time_group = Utils::GetTimeGroup(reporting_time);
    load_budget_key_timeframe_context.request->reporting_times = {
        reporting_time};
    shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group =
        make_shared<BudgetKeyTimeframeGroup>(time_group);

    load_budget_key_timeframe_context
        .callback = [&](AsyncContext<LoadBudgetKeyTimeframeRequest,
                                     LoadBudgetKeyTimeframeResponse>&
                            load_budget_key_timeframe_context) {
      if (result !=
          FailureExecutionResult(
              core::errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND)) {
        EXPECT_THAT(load_budget_key_timeframe_context.result, ResultIs(result));
        EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                      time_group, budget_key_timeframe_group),
                  SuccessExecutionResult());

        EXPECT_EQ(budget_key_timeframe_group->is_loaded.load(), false);
        EXPECT_EQ(budget_key_timeframe_group->needs_loader.load(), true);
      } else {
        EXPECT_EQ(load_budget_key_timeframe_context.result,
                  SuccessExecutionResult());
        EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                      time_group, budget_key_timeframe_group),
                  SuccessExecutionResult());
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->token_count,
            kMaxToken);
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->time_bucket_index,
            Utils::GetTimeBucket(reporting_time));
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->active_token_count.load(),
            0);
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->active_transaction_id.load()
                .low,
            0);
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->active_transaction_id.load()
                .high,
            0);

        EXPECT_EQ(budget_key_timeframe_group->needs_loader.load(), false);
        EXPECT_EQ(budget_key_timeframe_group->is_loaded.load(), true);
      }
      condition = true;
    };

    auto pair = make_pair(time_group, budget_key_timeframe_group);
    budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
        pair, budget_key_timeframe_group);

    budget_key_timeframe_group->is_loaded = false;

    budget_key_timeframe_manager.OnLoadTimeframeGroupFromDBCallback(
        load_budget_key_timeframe_context, budget_key_timeframe_group,
        get_database_item_context);

    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BudgetKeyTimeframeManagerTest,
     OnLoadTimeframeGroupFromDBCallbackSingleTimeframeRequest) {
  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context;

  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<NoSQLDatabaseProviderInterface>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  vector<string> token_attr_names = {"Token", "TokenCount", "TokenCount",
                                     "TokenCount", "TokenCount"};
  vector<string> token_attr_values = {
      "asd", "dsadasfa", "123 dsa 231 dsad 123",
      "a a a a a a a a a a a a a a a a a a a a a a a a",
      "1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"};

  for (size_t i = 0; i < token_attr_names.size(); ++i) {
    auto attr_name = token_attr_names[i];

    atomic<bool> condition(false);
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
        get_database_item_context;
    get_database_item_context.result = SuccessExecutionResult();
    get_database_item_context.response = make_shared<GetDatabaseItemResponse>();
    get_database_item_context.response->attributes =
        make_shared<std::vector<NoSqlDatabaseKeyValuePair>>();

    NoSqlDatabaseKeyValuePair nosql_attribute;
    nosql_attribute.attribute_name =
        make_shared<NoSQLDatabaseAttributeName>(attr_name);

    auto attr_value = token_attr_values[i];
    nosql_attribute.attribute_value =
        make_shared<NoSQLDatabaseValidAttributeValueTypes>(attr_value);

    get_database_item_context.response->attributes->push_back(nosql_attribute);

    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
        load_budget_key_timeframe_context;
    load_budget_key_timeframe_context.request =
        make_shared<LoadBudgetKeyTimeframeRequest>();
    Timestamp reporting_time = 1660498765350482296;
    TimeGroup time_group = Utils::GetTimeGroup(reporting_time);
    shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group =
        make_shared<BudgetKeyTimeframeGroup>(time_group);
    load_budget_key_timeframe_context.request->reporting_times = {
        reporting_time};
    load_budget_key_timeframe_context
        .callback = [&](AsyncContext<LoadBudgetKeyTimeframeRequest,
                                     LoadBudgetKeyTimeframeResponse>&
                            load_budget_key_timeframe_context) mutable {
      EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                    ->IsEvictable(time_group),
                true);
      if (i < 4) {
        EXPECT_EQ(
            load_budget_key_timeframe_context.result,
            FailureExecutionResult(
                core::errors::
                    SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA));
        EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                      time_group, budget_key_timeframe_group),
                  SuccessExecutionResult());
        EXPECT_EQ(budget_key_timeframe_group->needs_loader.load(), true);
        EXPECT_EQ(budget_key_timeframe_group->is_loaded.load(), false);
      } else {
        EXPECT_EQ(load_budget_key_timeframe_context.result,
                  SuccessExecutionResult());
        EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                      time_group, budget_key_timeframe_group),
                  SuccessExecutionResult());
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->token_count,
            1);
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->time_bucket_index,
            Utils::GetTimeBucket(reporting_time));
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->active_token_count.load(),
            0);
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->active_transaction_id.load()
                .low,
            0);
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->active_transaction_id.load()
                .high,
            0);

        EXPECT_EQ(budget_key_timeframe_group->needs_loader.load(), false);
        EXPECT_EQ(budget_key_timeframe_group->is_loaded.load(), true);
      }
      condition = true;
    };

    auto pair = make_pair(time_group, budget_key_timeframe_group);
    budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
        pair, budget_key_timeframe_group);

    budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
        ->DisableEviction(pair.first);

    EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                  ->IsEvictable(time_group),
              false);

    budget_key_timeframe_manager.OnLoadTimeframeGroupFromDBCallback(
        load_budget_key_timeframe_context, budget_key_timeframe_group,
        get_database_item_context);

    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BudgetKeyTimeframeManagerTest,
     OnLoadTimeframeGroupFromDBCallbackWithMultipleTimeframesRequest) {
  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context;

  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<NoSQLDatabaseProviderInterface>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  vector<string> token_attr_names = {"Token", "TokenCount", "TokenCount",
                                     "TokenCount", "TokenCount"};
  vector<string> token_attr_values = {
      "asd", "dsadasfa", "123 dsa 231 dsad 123",
      "a a a a a a a a a a a a a a a a a a a a a a a a",
      "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24"};

  for (size_t i = 0; i < token_attr_names.size(); ++i) {
    auto attr_name = token_attr_names[i];

    atomic<bool> condition(false);
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
        get_database_item_context;
    get_database_item_context.result = SuccessExecutionResult();
    get_database_item_context.response = make_shared<GetDatabaseItemResponse>();
    get_database_item_context.response->attributes =
        make_shared<std::vector<NoSqlDatabaseKeyValuePair>>();

    NoSqlDatabaseKeyValuePair nosql_attribute;
    nosql_attribute.attribute_name =
        make_shared<NoSQLDatabaseAttributeName>(attr_name);

    auto attr_value = token_attr_values[i];
    nosql_attribute.attribute_value =
        make_shared<NoSQLDatabaseValidAttributeValueTypes>(attr_value);

    get_database_item_context.response->attributes->push_back(nosql_attribute);

    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
        load_budget_key_timeframe_context;
    load_budget_key_timeframe_context.request =
        make_shared<LoadBudgetKeyTimeframeRequest>();

    Timestamp reporting_time1 = nanoseconds(0).count();
    Timestamp reporting_time2 = (nanoseconds(0) + hours(21)).count();
    TimeGroup time_group = Utils::GetTimeGroup(reporting_time1);
    shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group =
        make_shared<BudgetKeyTimeframeGroup>(time_group);
    load_budget_key_timeframe_context.request->reporting_times = {
        reporting_time1, reporting_time2};
    load_budget_key_timeframe_context
        .callback = [&](AsyncContext<LoadBudgetKeyTimeframeRequest,
                                     LoadBudgetKeyTimeframeResponse>&
                            load_budget_key_timeframe_context) mutable {
      EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                    ->IsEvictable(time_group),
                true);
      if (i < 4) {
        EXPECT_EQ(
            load_budget_key_timeframe_context.result,
            FailureExecutionResult(
                core::errors::
                    SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA));
        EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                      time_group, budget_key_timeframe_group),
                  SuccessExecutionResult());
        EXPECT_EQ(budget_key_timeframe_group->needs_loader.load(), true);
        EXPECT_EQ(budget_key_timeframe_group->is_loaded.load(), false);
      } else {
        EXPECT_EQ(load_budget_key_timeframe_context.result,
                  SuccessExecutionResult());
        EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                      time_group, budget_key_timeframe_group),
                  SuccessExecutionResult());
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->token_count,
            1);
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->time_bucket_index,
            Utils::GetTimeBucket(reporting_time1));
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->active_token_count.load(),
            0);
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->active_transaction_id.load()
                .low,
            0);
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[0]
                ->active_transaction_id.load()
                .high,
            0);

        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[1]
                ->token_count,
            22);
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[1]
                ->time_bucket_index,
            Utils::GetTimeBucket(reporting_time2));
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[1]
                ->active_token_count.load(),
            0);
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[1]
                ->active_transaction_id.load()
                .low,
            0);
        EXPECT_EQ(
            load_budget_key_timeframe_context.response->budget_key_frames[1]
                ->active_transaction_id.load()
                .high,
            0);

        EXPECT_EQ(budget_key_timeframe_group->needs_loader.load(), false);
        EXPECT_EQ(budget_key_timeframe_group->is_loaded.load(), true);
      }
      condition = true;
    };

    auto pair = make_pair(time_group, budget_key_timeframe_group);
    budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
        pair, budget_key_timeframe_group);

    budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
        ->DisableEviction(pair.first);

    EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                  ->IsEvictable(time_group),
              false);

    budget_key_timeframe_manager.OnLoadTimeframeGroupFromDBCallback(
        load_budget_key_timeframe_context, budget_key_timeframe_group,
        get_database_item_context);

    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BudgetKeyTimeframeManagerTest,
     OnLoadTimeframeGroupFromDBCallbackProperLogSerialization) {
  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context;

  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto mock_journal_service = make_shared<MockJournalService>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<NoSQLDatabaseProviderInterface>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  vector<string> token_attr_names = {"TokenCount"};
  vector<string> token_attr_values = {
      "1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"};

  for (size_t i = 0; i < token_attr_names.size(); ++i) {
    auto attr_name = token_attr_names[i];

    atomic<bool> condition(false);
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
        get_database_item_context;
    get_database_item_context.result = SuccessExecutionResult();
    get_database_item_context.response = make_shared<GetDatabaseItemResponse>();
    get_database_item_context.response->attributes =
        make_shared<std::vector<NoSqlDatabaseKeyValuePair>>();

    NoSqlDatabaseKeyValuePair nosql_attribute;
    nosql_attribute.attribute_name =
        make_shared<NoSQLDatabaseAttributeName>(attr_name);

    auto attr_value = token_attr_values[i];
    nosql_attribute.attribute_value =
        make_shared<NoSQLDatabaseValidAttributeValueTypes>(attr_value);

    get_database_item_context.response->attributes->push_back(nosql_attribute);

    AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
        load_budget_key_timeframe_context;
    load_budget_key_timeframe_context.request =
        make_shared<LoadBudgetKeyTimeframeRequest>();
    Timestamp reporting_time = 1660498765350482296;
    TimeGroup time_group = Utils::GetTimeGroup(reporting_time);

    mock_journal_service
        ->log_mock = [&](AsyncContext<JournalLogRequest, JournalLogResponse>&
                             journal_log_context) {
      EXPECT_EQ(journal_log_context.request->component_id.high,
                budget_key_timeframe_manager.GetId().high);
      EXPECT_EQ(journal_log_context.request->component_id.low,
                budget_key_timeframe_manager.GetId().low);
      EXPECT_EQ(journal_log_context.request->log_status, JournalLogStatus::Log);
      EXPECT_NE(journal_log_context.request->data->bytes->size(), 0);
      EXPECT_NE(journal_log_context.request->data->length, 0);
      EXPECT_NE(journal_log_context.request->data->capacity, 0);

      BudgetKeyTimeframeManagerLog budget_key_time_frame_manager_log;
      EXPECT_EQ(budget_key_timeframe_manager::Serialization::
                    DeserializeBudgetKeyTimeframeManagerLog(
                        *journal_log_context.request->data,
                        budget_key_time_frame_manager_log),
                SuccessExecutionResult());

      BudgetKeyTimeframeManagerLog_1_0 budget_key_time_frame_manager_log_1_0;
      EXPECT_EQ(budget_key_timeframe_manager::Serialization::
                    DeserializeBudgetKeyTimeframeManagerLog_1_0(
                        budget_key_time_frame_manager_log.log_body(),
                        budget_key_time_frame_manager_log_1_0),
                SuccessExecutionResult());

      EXPECT_EQ(budget_key_time_frame_manager_log_1_0.operation_type(),
                OperationType::INSERT_TIMEGROUP_INTO_CACHE);

      shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group;
      EXPECT_EQ(budget_key_timeframe_manager::Serialization::
                    DeserializeBudgetKeyTimeframeGroupLog_1_0(
                        budget_key_time_frame_manager_log_1_0.log_body(),
                        budget_key_timeframe_group),
                SuccessExecutionResult());

      EXPECT_EQ(time_group, budget_key_timeframe_group->time_group);

      vector<TimeGroup> old_keys(24, 1);
      vector<TimeGroup> new_keys;

      budget_key_timeframe_group->budget_key_timeframes.Keys(new_keys);

      EXPECT_EQ(old_keys.size(), new_keys.size());
      for (size_t i = 0; i < old_keys.size(); ++i) {
        shared_ptr<BudgetKeyTimeframe> budget_key_timeframe;

        EXPECT_EQ(budget_key_timeframe_group->budget_key_timeframes.Find(
                      old_keys[i], budget_key_timeframe),
                  SuccessExecutionResult());

        EXPECT_EQ(core::common::kZeroUuid,
                  budget_key_timeframe->active_transaction_id.load());
        EXPECT_EQ(1, budget_key_timeframe->token_count.load());
        EXPECT_EQ(0, budget_key_timeframe->active_token_count.load());
      }

      condition = true;
      return SuccessExecutionResult();
    };

    shared_ptr<BudgetKeyTimeframeGroup> budget_key_timeframe_group =
        make_shared<BudgetKeyTimeframeGroup>(time_group);
    load_budget_key_timeframe_context.request->reporting_times = {
        reporting_time};

    auto pair = make_pair(time_group, budget_key_timeframe_group);
    budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
        pair, budget_key_timeframe_group);

    budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
        ->DisableEviction(pair.first);

    EXPECT_EQ(budget_key_timeframe_manager.GetInternalBudgetTimeframeGroups()
                  ->IsEvictable(time_group),
              false);

    budget_key_timeframe_manager.OnLoadTimeframeGroupFromDBCallback(
        load_budget_key_timeframe_context, budget_key_timeframe_group,
        get_database_item_context);

    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(BudgetKeyTimeframeManagerTest, OnLogLoadCallbackFailure) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<NoSQLDatabaseProviderInterface>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  bool called = false;
  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context;
  load_budget_key_timeframe_context.callback =
      [&](AsyncContext<LoadBudgetKeyTimeframeRequest,
                       LoadBudgetKeyTimeframeResponse>&
              load_budget_key_timeframe_context) {
        EXPECT_EQ(load_budget_key_timeframe_context.result,
                  FailureExecutionResult(123));
        called = true;
      };

  auto budget_key_timeframe_group = make_shared<BudgetKeyTimeframeGroup>(1234);
  budget_key_timeframe_group->is_loaded = false;
  budget_key_timeframe_group->needs_loader = false;

  AsyncContext<JournalLogRequest, JournalLogResponse> journal_context;
  journal_context.result = FailureExecutionResult(123);

  budget_key_timeframe_manager.OnLogLoadCallback(
      load_budget_key_timeframe_context, budget_key_timeframe_group,
      journal_context);

  EXPECT_EQ(called, true);
  EXPECT_EQ(budget_key_timeframe_group->needs_loader, true);
  EXPECT_EQ(budget_key_timeframe_group->is_loaded, false);
}

TEST(BudgetKeyTimeframeManagerTest, OnLogLoadCallbackSuccess) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<NoSQLDatabaseProviderInterface>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  bool called = false;
  AsyncContext<LoadBudgetKeyTimeframeRequest, LoadBudgetKeyTimeframeResponse>
      load_budget_key_timeframe_context;
  load_budget_key_timeframe_context.callback =
      [&](AsyncContext<LoadBudgetKeyTimeframeRequest,
                       LoadBudgetKeyTimeframeResponse>&
              load_budget_key_timeframe_context) {
        EXPECT_EQ(load_budget_key_timeframe_context.result,
                  SuccessExecutionResult());
        called = true;
      };

  auto budget_key_timeframe_group = make_shared<BudgetKeyTimeframeGroup>(1234);
  budget_key_timeframe_group->is_loaded = false;
  budget_key_timeframe_group->needs_loader = false;

  AsyncContext<JournalLogRequest, JournalLogResponse> journal_context;
  journal_context.result = SuccessExecutionResult();

  budget_key_timeframe_manager.OnLogLoadCallback(
      load_budget_key_timeframe_context, budget_key_timeframe_group,
      journal_context);

  EXPECT_EQ(called, true);
  EXPECT_EQ(budget_key_timeframe_group->needs_loader, false);
  EXPECT_EQ(budget_key_timeframe_group->is_loaded, true);
}

TEST(BudgetKeyTimeframeManagerTest, OnStoreTimeframeGroupToDBCallbackFailure) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<NoSQLDatabaseProviderInterface>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context;
  upsert_database_item_context.result = FailureExecutionResult(1234);

  auto called = false;
  auto callback = [&](bool should_delete) {
    EXPECT_FALSE(should_delete);
    called = true;
  };

  shared_ptr<BudgetKeyTimeframeGroup> time_frame_group;
  TimeGroup time_group = 123;
  budget_key_timeframe_manager.OnStoreTimeframeGroupToDBCallback(
      upsert_database_item_context, time_group, time_frame_group, callback);

  EXPECT_TRUE(called);

  upsert_database_item_context.result = RetryExecutionResult(1234);
  called = false;
  budget_key_timeframe_manager.OnStoreTimeframeGroupToDBCallback(
      upsert_database_item_context, time_group, time_frame_group, callback);

  EXPECT_TRUE(called);
}

TEST(BudgetKeyTimeframeManagerTest, OnStoreTimeframeGroupToDBCallbackSuccess) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<NoSQLDatabaseProviderInterface>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context;
  upsert_database_item_context.result = SuccessExecutionResult();

  auto called = false;
  auto callback = [&](bool should_delete) { EXPECT_FALSE(true); };

  mock_journal_service->log_mock =
      [&](AsyncContext<JournalLogRequest, JournalLogResponse>&
              journal_log_context) {
        EXPECT_EQ(journal_log_context.request->component_id.high,
                  budget_key_timeframe_manager.GetId().high);
        EXPECT_EQ(journal_log_context.request->component_id.low,
                  budget_key_timeframe_manager.GetId().low);
        EXPECT_EQ(journal_log_context.request->log_status,
                  JournalLogStatus::Log);
        EXPECT_NE(journal_log_context.request->data->bytes->size(), 0);
        EXPECT_NE(journal_log_context.request->data->length, 0);
        EXPECT_NE(journal_log_context.request->data->capacity, 0);

        shared_ptr<BudgetKeyTimeframeGroup> time_frame_group;
        TimeGroup time_group = 123;
        EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                      time_group, time_frame_group),
                  SuccessExecutionResult());

        EXPECT_EQ(budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
                      journal_log_context.request->data, kDefaultUuid),
                  SuccessExecutionResult());

        EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                      time_group, time_frame_group),
                  FailureExecutionResult(
                      core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));

        called = true;
        return SuccessExecutionResult();
      };

  auto time_frame_group = make_shared<BudgetKeyTimeframeGroup>(123);
  TimeGroup time_group = 123;

  auto pair = make_pair(time_group, time_frame_group);
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      pair, time_frame_group);

  EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                time_group, time_frame_group),
            SuccessExecutionResult());

  budget_key_timeframe_manager.OnStoreTimeframeGroupToDBCallback(
      upsert_database_item_context, time_group, time_frame_group, callback);

  EXPECT_TRUE(called);
}

TEST(BudgetKeyTimeframeManagerTest, OnBeforeGarbageCollection) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  mock_config_provider->Set(kBudgetKeyTableName, string("PBS_BudgetKeys"));
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider = make_shared<MockNoSQLDatabaseProvider>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<MockNoSQLDatabaseProvider>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  bool is_called = false;
  mock_nosql_database_provider->upsert_database_item_mock =
      [&](AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
              upsert_database_item) {
        EXPECT_EQ(*upsert_database_item.request->table_name, "PBS_BudgetKeys");
        EXPECT_EQ(*upsert_database_item.request->partition_key->attribute_name,
                  "Budget_Key");
        EXPECT_EQ(
            get<string>(
                *upsert_database_item.request->partition_key->attribute_value),
            "budget_key_name");
        EXPECT_EQ(*upsert_database_item.request->sort_key->attribute_name,
                  "Timeframe");
        EXPECT_EQ(get<string>(
                      *upsert_database_item.request->sort_key->attribute_value),
                  "19218");

        EXPECT_EQ(
            *upsert_database_item.request->new_attributes->at(0).attribute_name,
            "TokenCount");
        EXPECT_EQ(
            get<string>(*upsert_database_item.request->new_attributes->at(0)
                             .attribute_value),
            "23 23 23 23 23 23 23 23 23 23 23 23 23 23 23 23 23 23 23 23 23 23 "
            "23 23");

        is_called = true;
        return SuccessExecutionResult();
      };

  Timestamp reporting_time = 1660498765350482296;
  auto time_group = Utils::GetTimeGroup(reporting_time);

  auto timeframe_group = make_shared<BudgetKeyTimeframeGroup>(time_group);

  for (int time_bucket = 0; time_bucket < 24; ++time_bucket) {
    auto timeframe = make_shared<BudgetKeyTimeframe>(time_bucket);
    timeframe->active_token_count = 1;
    timeframe->token_count = 23;
    timeframe->active_transaction_id = core::common::kZeroUuid;
    auto pair = make_pair(time_bucket, timeframe);

    timeframe_group->budget_key_timeframes.Insert(pair, timeframe);
  }

  auto group_pair = make_pair(time_group, timeframe_group);
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      group_pair, timeframe_group);

  auto should_delete = [&](bool should_delete) {};
  budget_key_timeframe_manager.OnBeforeGarbageCollection(
      time_group, timeframe_group, should_delete);
  EXPECT_EQ(is_called, true);
}

TEST(BudgetKeyTimeframeManagerTest,
     OnBeforeGarbageCollectionWithActiveTransactionId) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  mock_config_provider->Set(kBudgetKeyTableName, string("PBS_BudgetKeys"));
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider = make_shared<MockNoSQLDatabaseProvider>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<MockNoSQLDatabaseProvider>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  mock_journal_service->log_mock =
      [&](AsyncContext<JournalLogRequest, JournalLogResponse>&
              journal_log_context) { return FailureExecutionResult(1234); };

  Timestamp reporting_time = 1660498765350482296;
  auto time_group = Utils::GetTimeGroup(reporting_time);
  auto time_bucket = Utils::GetTimeBucket(reporting_time);

  auto timeframe_group = make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto timeframe = make_shared<BudgetKeyTimeframe>(time_bucket);
  timeframe->active_token_count = 1;
  timeframe->token_count = 23;
  timeframe->active_transaction_id = Uuid::GenerateUuid();
  auto pair = make_pair(time_bucket, timeframe);

  timeframe_group->budget_key_timeframes.Insert(pair, timeframe);
  bool called = false;
  auto newer_should_delete = [&](bool should_delete) {
    EXPECT_EQ(should_delete, false);
    called = true;
  };
  budget_key_timeframe_manager.OnBeforeGarbageCollection(
      time_group, timeframe_group, newer_should_delete);
  EXPECT_EQ(called, true);
}

TEST(BudgetKeyTimeframeManagerTest, OnRemoveEntryFromCacheLogged) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<NoSQLDatabaseProviderInterface>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  AsyncContext<JournalLogRequest, JournalLogResponse> journal_context;

  journal_context.result = FailureExecutionResult(123);
  function<void(bool)> should_delete = [](bool should_delete) {
    EXPECT_EQ(should_delete, false);
  };
  budget_key_timeframe_manager.OnRemoveEntryFromCacheLogged(should_delete,
                                                            journal_context);

  journal_context.result = RetryExecutionResult(123);
  should_delete = [](bool should_delete) { EXPECT_EQ(should_delete, false); };
  budget_key_timeframe_manager.OnRemoveEntryFromCacheLogged(should_delete,
                                                            journal_context);

  journal_context.result = SuccessExecutionResult();
  should_delete = [](bool should_delete) { EXPECT_EQ(should_delete, true); };
  budget_key_timeframe_manager.OnRemoveEntryFromCacheLogged(should_delete,
                                                            journal_context);
}

TEST(BudgetKeyTimeframeManagerTest, Checkpoint) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<NoSQLDatabaseProviderInterface>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  auto logs = make_shared<list<CheckpointLog>>();
  EXPECT_EQ(budget_key_timeframe_manager.Checkpoint(logs),
            SuccessExecutionResult());
  EXPECT_EQ(logs->size(), 0);

  Timestamp reporting_time_1 = 1660498765350482296;
  auto time_group_1 = Utils::GetTimeGroup(reporting_time_1);
  auto budget_key_timeframe_group_1 =
      make_shared<BudgetKeyTimeframeGroup>(time_group_1);
  auto timeframe_group_pair_1 =
      make_pair(time_group_1, budget_key_timeframe_group_1);
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      timeframe_group_pair_1, budget_key_timeframe_group_1);

  auto time_bucket_1 = Utils::GetTimeBucket(reporting_time_1);
  auto timeframe_1 = make_shared<BudgetKeyTimeframe>(time_bucket_1);
  timeframe_1->active_token_count = 1;
  timeframe_1->token_count = 23;
  timeframe_1->active_transaction_id = Uuid::GenerateUuid();
  auto pair_1 = make_pair(time_bucket_1, timeframe_1);
  budget_key_timeframe_group_1->budget_key_timeframes.Insert(pair_1,
                                                             timeframe_1);

  Timestamp reporting_time_2 = 1680498365350482296;
  auto time_group_2 = Utils::GetTimeGroup(reporting_time_2);
  auto budget_key_timeframe_group_2 =
      make_shared<BudgetKeyTimeframeGroup>(time_group_2);
  auto timeframe_group_pair_2 =
      make_pair(time_group_2, budget_key_timeframe_group_2);
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      timeframe_group_pair_2, budget_key_timeframe_group_2);

  auto time_bucket_2 = Utils::GetTimeBucket(reporting_time_2);
  auto timeframe_2 = make_shared<BudgetKeyTimeframe>(time_bucket_2);
  timeframe_2->active_token_count = 10;
  timeframe_2->token_count = 1;
  timeframe_2->active_transaction_id = Uuid::GenerateUuid();
  auto pair_2 = make_pair(time_bucket_2, timeframe_2);
  budget_key_timeframe_group_2->budget_key_timeframes.Insert(pair_2,
                                                             timeframe_2);
  EXPECT_EQ(budget_key_timeframe_manager.Checkpoint(logs),
            SuccessExecutionResult());
  EXPECT_EQ(logs->size(), 2);

  MockBudgetKeyTimeframeManager recovery_budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  auto it = logs->begin();
  auto bytes_buffer = make_shared<BytesBuffer>(it->bytes_buffer);
  EXPECT_EQ(it->component_id, budget_key_timeframe_manager.GetId());
  EXPECT_NE(it->log_id.low, 0);
  EXPECT_NE(it->log_id.high, 0);
  EXPECT_EQ(it->log_status, JournalLogStatus::Log);

  EXPECT_EQ(
      recovery_budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
          bytes_buffer, kDefaultUuid),
      SuccessExecutionResult());

  it++;
  bytes_buffer = make_shared<BytesBuffer>(it->bytes_buffer);
  EXPECT_EQ(it->component_id, budget_key_timeframe_manager.GetId());
  EXPECT_NE(it->log_id.low, 0);
  EXPECT_NE(it->log_id.high, 0);
  EXPECT_EQ(it->log_status, JournalLogStatus::Log);
  EXPECT_EQ(
      recovery_budget_key_timeframe_manager.OnJournalServiceRecoverCallback(
          bytes_buffer, kDefaultUuid),
      SuccessExecutionResult());
  it++;
  EXPECT_EQ(it, logs->end());

  vector<TimeGroup> original_timeframe_groups;
  vector<TimeGroup> recovered_timeframe_groups;

  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Keys(
      original_timeframe_groups);
  recovery_budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Keys(
      recovered_timeframe_groups);

  EXPECT_EQ(original_timeframe_groups.size(), 2);
  EXPECT_EQ(recovered_timeframe_groups.size(), 2);

  EXPECT_EQ(original_timeframe_groups[0] == recovered_timeframe_groups[0] ||
                original_timeframe_groups[1] == recovered_timeframe_groups[0],
            true);
  EXPECT_EQ(original_timeframe_groups[0] == recovered_timeframe_groups[1] ||
                original_timeframe_groups[1] == recovered_timeframe_groups[1],
            true);

  for (auto timeframe_group : original_timeframe_groups) {
    shared_ptr<BudgetKeyTimeframeGroup> original_budget_key_timeframe_group;
    shared_ptr<BudgetKeyTimeframeGroup> checkpoint_budget_key_timeframe_group;

    EXPECT_EQ(budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
                  timeframe_group, original_budget_key_timeframe_group),
              SuccessExecutionResult());
    EXPECT_EQ(
        recovery_budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Find(
            timeframe_group, checkpoint_budget_key_timeframe_group),
        SuccessExecutionResult());

    EXPECT_EQ(original_budget_key_timeframe_group->time_group,
              checkpoint_budget_key_timeframe_group->time_group);

    vector<TimeBucket> original_timeframe_buckets;
    vector<TimeBucket> recovered_timeframe_buckets;

    original_budget_key_timeframe_group->budget_key_timeframes.Keys(
        original_timeframe_buckets);
    checkpoint_budget_key_timeframe_group->budget_key_timeframes.Keys(
        recovered_timeframe_buckets);

    EXPECT_EQ(original_timeframe_buckets.size(), 1);
    EXPECT_EQ(recovered_timeframe_buckets.size(), 1);

    EXPECT_EQ(original_timeframe_buckets[0], recovered_timeframe_buckets[0]);

    shared_ptr<BudgetKeyTimeframe> original_budget_key_timeframe;
    original_budget_key_timeframe_group->budget_key_timeframes.Find(
        original_timeframe_buckets[0], original_budget_key_timeframe);

    shared_ptr<BudgetKeyTimeframe> checkpoint_budget_key_timeframe;
    checkpoint_budget_key_timeframe_group->budget_key_timeframes.Find(
        recovered_timeframe_buckets[0], checkpoint_budget_key_timeframe);

    EXPECT_EQ(original_budget_key_timeframe->active_token_count.load(),
              checkpoint_budget_key_timeframe->active_token_count.load());
    EXPECT_EQ(original_budget_key_timeframe->time_bucket_index,
              checkpoint_budget_key_timeframe->time_bucket_index);
    EXPECT_EQ(
        original_budget_key_timeframe->active_transaction_id.load().low,
        checkpoint_budget_key_timeframe->active_transaction_id.load().low);
    EXPECT_EQ(
        original_budget_key_timeframe->active_transaction_id.load().high,
        checkpoint_budget_key_timeframe->active_transaction_id.load().high);
    EXPECT_EQ(original_budget_key_timeframe->token_count,
              checkpoint_budget_key_timeframe->token_count);
  }
}

TEST(BudgetKeyTimeframeManagerTest, CanUnload) {
  auto mock_journal_service = make_shared<MockJournalService>();
  auto mock_metric_client = make_shared<MockMetricClient>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  auto journal_service =
      static_pointer_cast<JournalServiceInterface>(mock_journal_service);
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  auto budget_key_name = make_shared<string>("budget_key_name");
  Uuid id = Uuid::GenerateUuid();
  auto mock_nosql_database_provider =
      make_shared<MockNoSQLDatabaseProviderNoOverrides>();
  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      static_pointer_cast<NoSQLDatabaseProviderInterface>(
          mock_nosql_database_provider);

  MockBudgetKeyTimeframeManager budget_key_timeframe_manager(
      budget_key_name, id, async_executor, journal_service,
      nosql_database_provider, mock_metric_client, mock_config_provider);

  EXPECT_SUCCESS(budget_key_timeframe_manager.CanUnload());

  Timestamp reporting_time = 1660498765350482296;
  auto time_group = Utils::GetTimeGroup(reporting_time);
  auto time_bucket = Utils::GetTimeBucket(reporting_time);
  auto timeframe_group = make_shared<BudgetKeyTimeframeGroup>(time_group);
  auto timeframe_group_pair = make_pair(time_group, timeframe_group);
  budget_key_timeframe_manager.GetBudgetTimeframeGroups()->Insert(
      timeframe_group_pair, timeframe_group);

  auto timeframe = make_shared<BudgetKeyTimeframe>(time_bucket);
  timeframe->active_token_count = 1;
  timeframe->token_count = 23;
  timeframe->active_transaction_id = core::common::kZeroUuid;
  auto pair = make_pair(reporting_time, timeframe);
  timeframe_group->budget_key_timeframes.Insert(pair, timeframe);

  EXPECT_EQ(
      budget_key_timeframe_manager.CanUnload(),
      FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_CANNOT_BE_UNLOADED));
}

TEST(BudgetKeyTimeframeManagerTest, PopulateLoadBudgetKeyTimeframeResponse) {
  class BudgetKeyTimeframeManagerAcessor : public BudgetKeyTimeframeManager {
   public:
    static ExecutionResult PopulateLoadBudgetKeyTimeframeResponsePublic(
        const shared_ptr<BudgetKeyTimeframeGroup>& budget_key_timeframe_group,
        const shared_ptr<LoadBudgetKeyTimeframeRequest>&
            load_budget_key_timeframe_request,
        shared_ptr<LoadBudgetKeyTimeframeResponse>&
            load_budget_key_timeframe_response) {
      return PopulateLoadBudgetKeyTimeframeResponse(
          budget_key_timeframe_group, load_budget_key_timeframe_request,
          load_budget_key_timeframe_response);
    }
  };

  auto time_group = 0;
  auto time_bucket1 = Utils::GetTimeBucket(1);
  auto time_bucket2 = Utils::GetTimeBucket(4352352875);
  auto time_bucket3 = Utils::GetTimeBucket(8125125181251251);

  auto budget_key_timeframe_group =
      make_shared<BudgetKeyTimeframeGroup>(time_group);

  auto budget_key_timeframe1 = make_shared<BudgetKeyTimeframe>(time_bucket1);
  auto budget_key_timeframe2 = make_shared<BudgetKeyTimeframe>(time_bucket2);
  auto budget_key_timeframe3 = make_shared<BudgetKeyTimeframe>(time_bucket3);
  auto budget_key_timeframe_pair1 =
      make_pair(time_bucket1, budget_key_timeframe1);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      budget_key_timeframe_pair1, budget_key_timeframe1);
  auto budget_key_timeframe_pair2 =
      make_pair(time_bucket2, budget_key_timeframe2);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      budget_key_timeframe_pair2, budget_key_timeframe2);
  auto budget_key_timeframe_pair3 =
      make_pair(time_bucket3, budget_key_timeframe3);
  budget_key_timeframe_group->budget_key_timeframes.Insert(
      budget_key_timeframe_pair3, budget_key_timeframe3);

  {
    auto request = make_shared<LoadBudgetKeyTimeframeRequest>();
    request->reporting_times = {1};

    auto response = make_shared<LoadBudgetKeyTimeframeResponse>();
    auto execution_result = BudgetKeyTimeframeManagerAcessor::
        PopulateLoadBudgetKeyTimeframeResponsePublic(budget_key_timeframe_group,
                                                     request, response);
    EXPECT_SUCCESS(execution_result);

    EXPECT_EQ(response->budget_key_frames.size(), 1);
    EXPECT_EQ(response->budget_key_frames[0], budget_key_timeframe1);
  }

  {
    auto request = make_shared<LoadBudgetKeyTimeframeRequest>();
    request->reporting_times = {4352352875};

    auto response = make_shared<LoadBudgetKeyTimeframeResponse>();
    auto execution_result = BudgetKeyTimeframeManagerAcessor::
        PopulateLoadBudgetKeyTimeframeResponsePublic(budget_key_timeframe_group,
                                                     request, response);
    EXPECT_SUCCESS(execution_result);

    EXPECT_EQ(response->budget_key_frames.size(), 1);
    EXPECT_EQ(response->budget_key_frames[0], budget_key_timeframe2);
  }

  {
    auto request = make_shared<LoadBudgetKeyTimeframeRequest>();
    request->reporting_times = {8125125181251251};

    auto response = make_shared<LoadBudgetKeyTimeframeResponse>();
    auto execution_result = BudgetKeyTimeframeManagerAcessor::
        PopulateLoadBudgetKeyTimeframeResponsePublic(budget_key_timeframe_group,
                                                     request, response);
    EXPECT_SUCCESS(execution_result);

    EXPECT_EQ(response->budget_key_frames.size(), 1);
    EXPECT_EQ(response->budget_key_frames[0], budget_key_timeframe3);
  }

  {
    auto request = make_shared<LoadBudgetKeyTimeframeRequest>();
    request->reporting_times = {1, 8125125181251251};

    auto response = make_shared<LoadBudgetKeyTimeframeResponse>();
    auto execution_result = BudgetKeyTimeframeManagerAcessor::
        PopulateLoadBudgetKeyTimeframeResponsePublic(budget_key_timeframe_group,
                                                     request, response);
    EXPECT_SUCCESS(execution_result);

    EXPECT_EQ(response->budget_key_frames.size(), 2);
    EXPECT_EQ(response->budget_key_frames[0], budget_key_timeframe1);
    EXPECT_EQ(response->budget_key_frames[1], budget_key_timeframe3);
  }

  {
    auto request = make_shared<LoadBudgetKeyTimeframeRequest>();
    request->reporting_times = {8125125181251251, 1};

    auto response = make_shared<LoadBudgetKeyTimeframeResponse>();
    auto execution_result = BudgetKeyTimeframeManagerAcessor::
        PopulateLoadBudgetKeyTimeframeResponsePublic(budget_key_timeframe_group,
                                                     request, response);
    EXPECT_SUCCESS(execution_result);

    EXPECT_EQ(response->budget_key_frames.size(), 2);
    EXPECT_EQ(response->budget_key_frames[0], budget_key_timeframe3);
    EXPECT_EQ(response->budget_key_frames[1], budget_key_timeframe1);
  }
}
}  // namespace google::scp::pbs::test
