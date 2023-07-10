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

#include "core/journal_service/src/journal_service.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/common/concurrent_map/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/journal_service/mock/mock_journal_input_stream.h"
#include "core/journal_service/mock/mock_journal_output_stream.h"
#include "core/journal_service/mock/mock_journal_service_with_overrides.h"
#include "core/journal_service/src/error_codes.h"
#include "core/journal_service/src/proto/journal_service.pb.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/metric_client_provider/mock/mock_metric_client_provider.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::JournalService;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::blob_storage_provider::mock::MockBlobStorageProvider;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::journal_service::JournalInputStreamInterface;
using google::scp::core::journal_service::JournalLog;
using google::scp::core::journal_service::JournalOutputStreamInterface;
using google::scp::core::journal_service::JournalStreamAppendLogRequest;
using google::scp::core::journal_service::JournalStreamAppendLogResponse;
using google::scp::core::journal_service::JournalStreamReadLogObject;
using google::scp::core::journal_service::JournalStreamReadLogRequest;
using google::scp::core::journal_service::JournalStreamReadLogResponse;
using google::scp::core::journal_service::mock::MockJournalInputStream;
using google::scp::core::journal_service::mock::MockJournalOutputStream;
using google::scp::core::journal_service::mock::MockJournalServiceWithOverrides;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::TimeEvent;
using google::scp::cpio::client_providers::mock::MockMetricClientProvider;
using google::scp::cpio::client_providers::mock::MockSimpleMetric;
using std::atomic;
using std::make_pair;
using std::make_shared;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::unordered_set;
using std::vector;

namespace google::scp::core::test {
TEST(JournalServiceTests, Init) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  JournalService journal_service(bucket_name, partition_name, async_executor,
                                 blob_storage_provider, mock_metric_client,
                                 mock_config_provider);

  EXPECT_EQ(journal_service.Init(), SuccessExecutionResult());
  EXPECT_EQ(
      journal_service.Init(),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_ALREADY_INITIALIZED));
}

TEST(JournalServiceTests, Run) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  JournalService journal_service(bucket_name, partition_name, async_executor,
                                 blob_storage_provider, mock_metric_client,
                                 mock_config_provider);

  EXPECT_EQ(journal_service.Run(),
            FailureExecutionResult(errors::SC_JOURNAL_SERVICE_NOT_INITIALIZED));
  EXPECT_EQ(journal_service.Init(), SuccessExecutionResult());
  EXPECT_EQ(journal_service.Run(), SuccessExecutionResult());
  EXPECT_EQ(journal_service.Run(),
            FailureExecutionResult(errors::SC_JOURNAL_SERVICE_ALREADY_RUNNING));
  EXPECT_EQ(journal_service.Stop(), SuccessExecutionResult());
}

TEST(JournalServiceTests, Stop) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  JournalService journal_service(bucket_name, partition_name, async_executor,
                                 blob_storage_provider, mock_metric_client,
                                 mock_config_provider);

  EXPECT_EQ(journal_service.Stop(),
            FailureExecutionResult(errors::SC_JOURNAL_SERVICE_ALREADY_STOPPED));
  EXPECT_EQ(journal_service.Init(), SuccessExecutionResult());
  EXPECT_EQ(journal_service.Run(), SuccessExecutionResult());
  EXPECT_EQ(journal_service.Stop(), SuccessExecutionResult());
  EXPECT_EQ(journal_service.Stop(),
            FailureExecutionResult(errors::SC_JOURNAL_SERVICE_ALREADY_STOPPED));
}

TEST(JournalServiceTests, Recover) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  MockJournalServiceWithOverrides journal_service(
      bucket_name, partition_name, async_executor, blob_storage_provider,
      mock_metric_client, mock_config_provider);
  shared_ptr<BlobStorageClientInterface> blob_storage_client;
  blob_storage_provider->CreateBlobStorageClient(blob_storage_client);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(12345)};
  for (auto result : results) {
    auto mock_input_stream = make_shared<MockJournalInputStream>(
        bucket_name, partition_name, blob_storage_client);

    mock_input_stream->read_log_mock =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>& read_log_context) {
          return result;
        };

    shared_ptr<JournalInputStreamInterface> input_stream =
        static_pointer_cast<JournalInputStreamInterface>(mock_input_stream);
    journal_service.SetInputStream(input_stream);

    AsyncContext<JournalRecoverRequest, JournalRecoverResponse>
        journal_recover_context;
    journal_recover_context.request = make_shared<JournalRecoverRequest>();

    EXPECT_EQ(journal_service.Recover(journal_recover_context), result);
  }
}

TEST(JournalServiceTests, OnJournalStreamReadLogCallbackStreamFailure) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  MockJournalServiceWithOverrides journal_service(
      bucket_name, partition_name, async_executor, blob_storage_provider,
      mock_metric_client, mock_config_provider);
  shared_ptr<BlobStorageClientInterface> blob_storage_client;
  blob_storage_provider->CreateBlobStorageClient(blob_storage_client);

  auto mock_input_stream = make_shared<MockJournalInputStream>(
      bucket_name, partition_name, blob_storage_client);
  mock_input_stream->SetLastProcessedJournalId(12345);
  auto input_stream =
      static_pointer_cast<JournalInputStreamInterface>(mock_input_stream);
  journal_service.SetInputStream(input_stream);

  AsyncContext<JournalRecoverRequest, JournalRecoverResponse>
      journal_recover_context;
  journal_recover_context.callback =
      [](AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
             journal_recover_context) {
        EXPECT_EQ(journal_recover_context.result, FailureExecutionResult(123));
      };

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      read_log_context;
  read_log_context.result = FailureExecutionResult(123);

  auto time_event = make_shared<TimeEvent>();
  auto replayed_logs = make_shared<unordered_set<string>>();
  journal_service.OnJournalStreamReadLogCallback(
      time_event, replayed_logs, journal_recover_context, read_log_context);

  EXPECT_EQ(journal_service.GetOutputStream(), nullptr);

  read_log_context.result = FailureExecutionResult(
      errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN);

  journal_recover_context.callback =
      [](AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
             journal_recover_context) {
        EXPECT_EQ(journal_recover_context.result, SuccessExecutionResult());
        EXPECT_EQ(journal_recover_context.response->last_processed_journal_id,
                  12345);
      };

  journal_service.OnJournalStreamReadLogCallback(
      time_event, replayed_logs, journal_recover_context, read_log_context);

  EXPECT_NE(journal_service.GetOutputStream(), nullptr);
}

TEST(JournalServiceTests, OnJournalStreamReadLogCallbackNoCallbackFound) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  MockJournalServiceWithOverrides journal_service(
      bucket_name, partition_name, async_executor, blob_storage_provider,
      mock_metric_client, mock_config_provider);
  shared_ptr<BlobStorageClientInterface> blob_storage_client;
  blob_storage_provider->CreateBlobStorageClient(blob_storage_client);

  AsyncContext<JournalRecoverRequest, JournalRecoverResponse>
      journal_recover_context;
  journal_recover_context.callback =
      [](AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
             journal_recover_context) {
        EXPECT_EQ(journal_recover_context.result,
                  FailureExecutionResult(
                      errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));
      };

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      read_log_context;
  read_log_context.response = make_shared<JournalStreamReadLogResponse>();
  read_log_context.response->read_logs =
      make_shared<vector<JournalStreamReadLogObject>>();
  JournalStreamReadLogObject log_object;
  log_object.log_id = Uuid::GenerateUuid();
  read_log_context.response->read_logs->push_back(log_object);
  read_log_context.result = SuccessExecutionResult();
  auto time_event = make_shared<TimeEvent>();
  auto replayed_logs = make_shared<unordered_set<string>>();
  journal_service.OnJournalStreamReadLogCallback(
      time_event, replayed_logs, journal_recover_context, read_log_context);
}

TEST(JournalServiceTests,
     OnJournalStreamReadLogCallbackCallbackFoundWithFailure) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  MockJournalServiceWithOverrides journal_service(
      bucket_name, partition_name, async_executor, blob_storage_provider,
      mock_metric_client, mock_config_provider);
  shared_ptr<BlobStorageClientInterface> blob_storage_client;
  blob_storage_provider->CreateBlobStorageClient(blob_storage_client);

  atomic<bool> called = false;
  AsyncContext<JournalRecoverRequest, JournalRecoverResponse>
      journal_recover_context;
  journal_recover_context.callback =
      [&](AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
              journal_recover_context) {
        EXPECT_EQ(journal_recover_context.result, FailureExecutionResult(123));
        called = true;
      };

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      read_log_context;
  read_log_context.response = make_shared<JournalStreamReadLogResponse>();
  read_log_context.response->read_logs =
      make_shared<vector<JournalStreamReadLogObject>>();
  JournalStreamReadLogObject log_object;
  log_object.log_id = Uuid::GenerateUuid();
  log_object.component_id = Uuid::GenerateUuid();
  log_object.journal_log = make_shared<JournalLog>();
  read_log_context.response->read_logs->push_back(log_object);
  read_log_context.result = SuccessExecutionResult();

  std::function<core::ExecutionResult(
      const std::shared_ptr<core::BytesBuffer>&)>
      callback = [](const std::shared_ptr<core::BytesBuffer>&) {
        return FailureExecutionResult(123);
      };

  auto pair = make_pair(
      read_log_context.response->read_logs->at(0).component_id, callback);
  journal_service.GetSubscribersMap().Insert(pair, callback);

  auto time_event = make_shared<TimeEvent>();
  auto replayed_logs = make_shared<unordered_set<string>>();
  journal_service.OnJournalStreamReadLogCallback(
      time_event, replayed_logs, journal_recover_context, read_log_context);

  WaitUntil([&]() { return called.load(); });
}

TEST(JournalServiceTests,
     OnJournalStreamReadLogCallbackCallbackFoundWithSuccess) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  MockJournalServiceWithOverrides journal_service(
      bucket_name, partition_name, async_executor, blob_storage_provider,
      mock_metric_client, mock_config_provider);
  shared_ptr<BlobStorageClientInterface> blob_storage_client;
  blob_storage_provider->CreateBlobStorageClient(blob_storage_client);

  atomic<bool> called = false;
  AsyncContext<JournalRecoverRequest, JournalRecoverResponse>
      journal_recover_context;
  journal_recover_context.callback =
      [&](AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
              journal_recover_context) {
        EXPECT_EQ(journal_recover_context.result, SuccessExecutionResult());
      };

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      read_log_context;
  read_log_context.response = make_shared<JournalStreamReadLogResponse>();
  read_log_context.response->read_logs =
      make_shared<vector<JournalStreamReadLogObject>>();
  JournalStreamReadLogObject log_object;
  log_object.log_id = Uuid::GenerateUuid();
  log_object.component_id = Uuid::GenerateUuid();
  log_object.journal_log = make_shared<JournalLog>();
  read_log_context.response->read_logs->push_back(log_object);
  read_log_context.result = SuccessExecutionResult();

  size_t call_count = 0;

  std::function<core::ExecutionResult(
      const std::shared_ptr<core::BytesBuffer>&)>
      callback = [&](const std::shared_ptr<core::BytesBuffer>&) {
        ExecutionResult execution_result;
        if (call_count++ == 0) {
          execution_result = SuccessExecutionResult();
        } else {
          EXPECT_EQ(true, false);
          execution_result = FailureExecutionResult(123);
        }
        return execution_result;
      };

  auto mock_input_stream = make_shared<MockJournalInputStream>(
      bucket_name, partition_name, blob_storage_client);

  mock_input_stream->read_log_mock =
      [&](AsyncContext<JournalStreamReadLogRequest,
                       JournalStreamReadLogResponse>& read_log_context) {
        called = true;
        return SuccessExecutionResult();
      };

  shared_ptr<JournalInputStreamInterface> input_stream =
      static_pointer_cast<JournalInputStreamInterface>(mock_input_stream);
  journal_service.SetInputStream(input_stream);

  auto pair = make_pair(
      read_log_context.response->read_logs->at(0).component_id, callback);
  journal_service.GetSubscribersMap().Insert(pair, callback);

  auto time_event = make_shared<TimeEvent>();
  auto replayed_logs = make_shared<unordered_set<string>>();
  journal_service.OnJournalStreamReadLogCallback(
      time_event, replayed_logs, journal_recover_context, read_log_context);

  WaitUntil([&]() { return called.load(); });
  EXPECT_EQ(replayed_logs->size(), 1);

  // Duplicated logs will not be replayed.
  called = false;
  journal_service.OnJournalStreamReadLogCallback(
      time_event, replayed_logs, journal_recover_context, read_log_context);
  WaitUntil([&]() { return called.load(); });
  EXPECT_EQ(replayed_logs->size(), 1);
}

TEST(JournalServiceTests, OnJournalStreamAppendLogCallback) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  MockJournalServiceWithOverrides journal_service(
      bucket_name, partition_name, async_executor, blob_storage_provider,
      mock_metric_client, mock_config_provider);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(12345)};
  for (auto result : results) {
    AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
    journal_log_context.callback =
        [&](AsyncContext<JournalLogRequest, JournalLogResponse>&
                journal_log_context) {
          EXPECT_EQ(journal_log_context.result, result);
        };
    AsyncContext<journal_service::JournalStreamAppendLogRequest,
                 journal_service::JournalStreamAppendLogResponse>
        write_journal_stream_context;
    write_journal_stream_context.result = result;

    journal_service.OnJournalStreamAppendLogCallback(
        journal_log_context, write_journal_stream_context);
  }
}

TEST(JournalServiceTests, SubscribeForRecovery) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  {
    MockJournalServiceWithOverrides journal_service(
        bucket_name, partition_name, async_executor, blob_storage_provider,
        mock_metric_client, mock_config_provider);

    journal_service.Init();
    journal_service.Run();
    std::function<core::ExecutionResult(
        const std::shared_ptr<core::BytesBuffer>&)>
        callback = [](const std::shared_ptr<core::BytesBuffer>&) {
          return FailureExecutionResult(123);
        };
    EXPECT_EQ(
        journal_service.SubscribeForRecovery(Uuid::GenerateUuid(), callback),
        FailureExecutionResult(
            errors::SC_JOURNAL_SERVICE_CANNOT_SUBSCRIBE_WHEN_RUNNING));

    journal_service.Stop();
  }

  {
    auto mock_config_provider = make_shared<MockConfigProvider>();
    MockJournalServiceWithOverrides journal_service(
        bucket_name, partition_name, async_executor, blob_storage_provider,
        mock_metric_client, mock_config_provider);

    journal_service.Init();
    std::function<core::ExecutionResult(
        const std::shared_ptr<core::BytesBuffer>&)>
        callback = [](const std::shared_ptr<core::BytesBuffer>&) {
          return FailureExecutionResult(123);
        };

    auto id = Uuid::GenerateUuid();
    EXPECT_EQ(journal_service.SubscribeForRecovery(id, callback),
              SuccessExecutionResult());
    EXPECT_EQ(journal_service.GetSubscribersMap().Find(id, callback),
              SuccessExecutionResult());
    journal_service.Stop();
  }

  {
    MockJournalServiceWithOverrides journal_service(
        bucket_name, partition_name, async_executor, blob_storage_provider,
        mock_metric_client, mock_config_provider);

    journal_service.Init();
    std::function<core::ExecutionResult(
        const std::shared_ptr<core::BytesBuffer>&)>
        callback = [](const std::shared_ptr<core::BytesBuffer>&) {
          return FailureExecutionResult(123);
        };

    auto id = Uuid::GenerateUuid();
    EXPECT_EQ(journal_service.SubscribeForRecovery(id, callback),
              SuccessExecutionResult());
    EXPECT_EQ(
        journal_service.SubscribeForRecovery(id, callback),
        FailureExecutionResult(errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS));
    journal_service.Stop();
  }
}

TEST(JournalServiceTests, UnsubscribeForRecovery) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto mock_config_provider = make_shared<MockConfigProvider>();
  {
    MockJournalServiceWithOverrides journal_service(
        bucket_name, partition_name, async_executor, blob_storage_provider,
        mock_metric_client, mock_config_provider);

    journal_service.Init();
    journal_service.Run();

    EXPECT_EQ(journal_service.UnsubscribeForRecovery(Uuid::GenerateUuid()),
              FailureExecutionResult(
                  errors::SC_JOURNAL_SERVICE_CANNOT_UNSUBSCRIBE_WHEN_RUNNING));

    journal_service.Stop();
  }

  {
    MockJournalServiceWithOverrides journal_service(
        bucket_name, partition_name, async_executor, blob_storage_provider,
        mock_metric_client, mock_config_provider);

    journal_service.Init();
    std::function<core::ExecutionResult(
        const std::shared_ptr<core::BytesBuffer>&)>
        callback = [](const std::shared_ptr<core::BytesBuffer>&) {
          return FailureExecutionResult(123);
        };

    auto id = Uuid::GenerateUuid();
    EXPECT_EQ(journal_service.SubscribeForRecovery(id, callback),
              SuccessExecutionResult());
    EXPECT_EQ(journal_service.UnsubscribeForRecovery(id),
              SuccessExecutionResult());
    EXPECT_EQ(
        journal_service.GetSubscribersMap().Find(id, callback),
        FailureExecutionResult(errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));
    journal_service.Stop();
  }

  {
    auto mock_config_provider = make_shared<MockConfigProvider>();
    MockJournalServiceWithOverrides journal_service(
        bucket_name, partition_name, async_executor, blob_storage_provider,
        mock_metric_client, mock_config_provider);

    journal_service.Init();
    std::function<core::ExecutionResult(
        const std::shared_ptr<core::BytesBuffer>&)>
        callback = [](const std::shared_ptr<core::BytesBuffer>&) {
          return FailureExecutionResult(123);
        };

    auto id = Uuid::GenerateUuid();
    EXPECT_EQ(
        journal_service.UnsubscribeForRecovery(id),
        FailureExecutionResult(errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST));
    journal_service.Stop();
  }
}
}  // namespace google::scp::core::test
