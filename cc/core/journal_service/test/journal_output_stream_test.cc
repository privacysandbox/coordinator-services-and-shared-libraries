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

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/journal_service/mock/mock_journal_output_stream.h"
#include "core/journal_service/src/journal_serialization.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::blob_storage_provider::mock::MockBlobStorageClient;
using google::scp::core::common::Uuid;
using google::scp::core::journal_service::JournalLog;
using google::scp::core::journal_service::JournalSerialization;
using google::scp::core::journal_service::JournalStreamAppendLogRequest;
using google::scp::core::journal_service::JournalStreamAppendLogResponse;
using google::scp::core::journal_service::mock::MockJournalOutputStream;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::function;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::core::test {
TEST(JournalOutputStreamTests, AppendLog) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockAsyncExecutor async_executor_mock;
  async_executor_mock.schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp,
          function<bool()>& cancellation_callback) {
        return SuccessExecutionResult();
      };
  async_executor_mock.schedule_mock = [&](const AsyncOperation& work) {
    return SuccessExecutionResult();
  };

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(async_executor_mock));
  // When the result of buffer creation is success but buffer does not have
  // enough space.
  MockBlobStorageClient mock_storage_client;
  mock_storage_client.put_blob_mock =
      [&](AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) {
        return SuccessExecutionResult();
      };
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalOutputStream mock_journal_output_stream(
      bucket_name, partition_name, async_executor, storage_client);

  AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>
      journal_stream_append_log_context;
  journal_stream_append_log_context.request =
      make_shared<JournalStreamAppendLogRequest>();
  journal_stream_append_log_context.request->journal_log =
      make_shared<JournalLog>();

  for (int i = 0; i < 5; ++i) {
    EXPECT_SUCCESS(mock_journal_output_stream.AppendLog(
        journal_stream_append_log_context));
  }

  EXPECT_EQ(mock_journal_output_stream.GetPendingLogsCount().load(), 5);
  EXPECT_EQ(mock_journal_output_stream.GetPendingLogs().Size(), 5);
}

TEST(JournalOutputStreamTests, FlushLogsFailure) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockAsyncExecutor async_executor_mock;
  async_executor_mock.schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp,
          function<bool()>& cancellation_callback) {
        return SuccessExecutionResult();
      };
  async_executor_mock.schedule_mock = [&](const AsyncOperation& work) {
    return SuccessExecutionResult();
  };

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(async_executor_mock));
  // When the result of buffer creation is success but buffer does not have
  // enough space.
  MockBlobStorageClient mock_storage_client;
  mock_storage_client.put_blob_mock =
      [&](AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) {
        return SuccessExecutionResult();
      };
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalOutputStream mock_journal_output_stream(
      bucket_name, partition_name, async_executor, storage_client);

  mock_journal_output_stream.create_new_buffer_mock = []() {
    return FailureExecutionResult(123);
  };

  AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>
      journal_stream_append_log_context;
  journal_stream_append_log_context.request =
      make_shared<JournalStreamAppendLogRequest>();
  journal_stream_append_log_context.request->journal_log =
      make_shared<JournalLog>();

  EXPECT_EQ(mock_journal_output_stream.GetPendingLogsCount().load(), 0);
  EXPECT_EQ(mock_journal_output_stream.GetPendingLogs().Size(), 0);
  EXPECT_SUCCESS(mock_journal_output_stream.FlushLogs());

  for (int i = 0; i < 5; ++i) {
    EXPECT_SUCCESS(mock_journal_output_stream.AppendLog(
        journal_stream_append_log_context));
  }

  EXPECT_THAT(mock_journal_output_stream.FlushLogs(),
              ResultIs(FailureExecutionResult(123)));
}

TEST(JournalOutputStreamTests, FlushLogsSchedulingFailure) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockAsyncExecutor async_executor_mock;

  async_executor_mock.schedule_mock = [](auto work) {
    return FailureExecutionResult(123);
  };

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(async_executor_mock));
  // When the result of buffer creation is success but buffer does not have
  // enough space.
  MockBlobStorageClient mock_storage_client;
  mock_storage_client.put_blob_mock =
      [&](AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) {
        return SuccessExecutionResult();
      };
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalOutputStream mock_journal_output_stream(
      bucket_name, partition_name, async_executor, storage_client);

  AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>
      journal_stream_append_log_context;
  journal_stream_append_log_context.request =
      make_shared<JournalStreamAppendLogRequest>();
  journal_stream_append_log_context.request->journal_log =
      make_shared<JournalLog>();

  int count = 0;
  journal_stream_append_log_context.callback = [&](auto& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(123)));
    count++;
  };

  EXPECT_EQ(mock_journal_output_stream.GetPendingLogsCount().load(), 0);
  EXPECT_EQ(mock_journal_output_stream.GetPendingLogs().Size(), 0);
  EXPECT_SUCCESS(mock_journal_output_stream.FlushLogs());

  for (int i = 0; i < 5; ++i) {
    EXPECT_SUCCESS(mock_journal_output_stream.AppendLog(
        journal_stream_append_log_context));
  }

  EXPECT_THAT(mock_journal_output_stream.FlushLogs(),
              ResultIs(FailureExecutionResult(123)));

  EXPECT_EQ(count, 5);
}

TEST(JournalOutputStreamTests, WriteBatch) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockAsyncExecutor async_executor_mock;

  async_executor_mock.schedule_mock = [](auto work) {
    work();
    return SuccessExecutionResult();
  };

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(async_executor_mock));
  // When the result of buffer creation is success but buffer does not have
  // enough space.
  MockBlobStorageClient mock_storage_client;
  mock_storage_client.put_blob_mock =
      [&](AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) {
        return SuccessExecutionResult();
      };
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalOutputStream mock_journal_output_stream(
      bucket_name, partition_name, async_executor, storage_client);

  bool is_called = false;
  mock_journal_output_stream.write_back_mock = [&](auto& flush_batch,
                                                   auto journal_id) {
    EXPECT_NE(journal_id, 0);
    EXPECT_EQ(flush_batch->size(), 5);
    is_called = true;
  };

  AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>
      journal_stream_append_log_context;
  journal_stream_append_log_context.request =
      make_shared<JournalStreamAppendLogRequest>();
  journal_stream_append_log_context.request->journal_log =
      make_shared<JournalLog>();

  EXPECT_EQ(mock_journal_output_stream.GetPendingLogsCount().load(), 0);
  EXPECT_EQ(mock_journal_output_stream.GetPendingLogs().Size(), 0);
  EXPECT_SUCCESS(mock_journal_output_stream.FlushLogs());

  for (int i = 0; i < 5; ++i) {
    EXPECT_SUCCESS(mock_journal_output_stream.AppendLog(
        journal_stream_append_log_context));
  }

  EXPECT_SUCCESS(mock_journal_output_stream.FlushLogs());
  EXPECT_TRUE(is_called);
}

TEST(JournalOutputStreamTests, WriteBatchWriteBlobFailure) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockAsyncExecutor async_executor_mock;

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(async_executor_mock));
  // When the result of buffer creation is success but buffer does not have
  // enough space.
  MockBlobStorageClient mock_storage_client;
  mock_storage_client.put_blob_mock =
      [&](AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) {
        return SuccessExecutionResult();
      };
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalOutputStream mock_journal_output_stream(
      bucket_name, partition_name, async_executor, storage_client);
  bool is_called = false;
  mock_journal_output_stream.write_journal_blob_mock = [&](auto a, auto b,
                                                           auto c) {
    is_called = true;
    return FailureExecutionResult(123);
  };

  AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>
      journal_stream_append_log_context;
  journal_stream_append_log_context.request =
      make_shared<JournalStreamAppendLogRequest>();
  journal_stream_append_log_context.request->journal_log =
      make_shared<JournalLog>();
  int count = 0;
  journal_stream_append_log_context.callback = [&](auto& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(123)));
    count++;
  };

  EXPECT_EQ(mock_journal_output_stream.GetPendingLogsCount().load(), 0);
  EXPECT_EQ(mock_journal_output_stream.GetPendingLogs().Size(), 0);
  EXPECT_SUCCESS(mock_journal_output_stream.FlushLogs());

  for (int i = 0; i < 5; ++i) {
    EXPECT_SUCCESS(mock_journal_output_stream.AppendLog(
        journal_stream_append_log_context));
  }

  EXPECT_SUCCESS(mock_journal_output_stream.FlushLogs());
  EXPECT_TRUE(is_called);
  EXPECT_EQ(count, 5);
}

TEST(JournalOutputStreamTests, GetSerializedLogByteSize) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockAsyncExecutor async_executor_mock;
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(async_executor_mock));

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalOutputStream mock_journal_output_stream(
      bucket_name, partition_name, async_executor, storage_client);

  AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>
      journal_stream_append_log_context;
  journal_stream_append_log_context.request =
      make_shared<JournalStreamAppendLogRequest>();
  journal_stream_append_log_context.request->journal_log =
      make_shared<JournalLog>();

  EXPECT_EQ(
      mock_journal_output_stream.GetSerializedLogByteSize(
          journal_stream_append_log_context),
      journal_stream_append_log_context.request->journal_log->ByteSizeLong() +
          sizeof(uint64_t) + kLogHeaderByteLength);
}

TEST(JournalOutputStreamTests, SerializeLog) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockAsyncExecutor async_executor_mock;
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(async_executor_mock));

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalOutputStream mock_journal_output_stream(
      bucket_name, partition_name, async_executor, storage_client);

  AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>
      journal_stream_append_log_context;
  journal_stream_append_log_context.request =
      make_shared<JournalStreamAppendLogRequest>();
  journal_stream_append_log_context.request->journal_log =
      make_shared<JournalLog>();

  journal_stream_append_log_context.request->journal_log->set_type(1234);
  journal_stream_append_log_context.request->log_id.high = 12345;
  journal_stream_append_log_context.request->log_id.low = 54321;
  journal_stream_append_log_context.request->component_id.high = 98765;
  journal_stream_append_log_context.request->component_id.low = 56789;
  journal_stream_append_log_context.request->log_status = JournalLogStatus::Log;

  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(1000);
  bytes_buffer.length = 0;
  bytes_buffer.capacity = 1000;
  size_t bytes_serialized = 0;
  EXPECT_EQ(
      mock_journal_output_stream.SerializeLog(journal_stream_append_log_context,
                                              bytes_buffer, bytes_serialized),
      SuccessExecutionResult());
  bytes_buffer.length += bytes_serialized;

  JournalLog journal_log =
      (*journal_stream_append_log_context.request->journal_log);
  EXPECT_EQ(bytes_serialized, kLogHeaderByteLength + sizeof(uint64_t) +
                                  journal_log.ByteSizeLong());

  JournalLog deserialized_journal_log;
  size_t bytes_deserialized = 0;
  size_t total_bytes_deserialized = 0;
  Uuid log_id;
  Uuid component_id;
  JournalLogStatus log_status;
  Timestamp timestamp;
  EXPECT_EQ(JournalSerialization::DeserializeLogHeader(
                bytes_buffer, 0, timestamp, log_status, component_id, log_id,
                bytes_deserialized),
            SuccessExecutionResult());
  EXPECT_EQ(kLogHeaderByteLength, bytes_deserialized);
  EXPECT_EQ(log_status, journal_stream_append_log_context.request->log_status);
  EXPECT_EQ(log_id, journal_stream_append_log_context.request->log_id);
  EXPECT_EQ(component_id,
            journal_stream_append_log_context.request->component_id);
  EXPECT_NE(timestamp, 0);

  total_bytes_deserialized += bytes_deserialized;
  bytes_deserialized = 0;

  EXPECT_EQ(JournalSerialization::DeserializeJournalLog(
                bytes_buffer, kLogHeaderByteLength, deserialized_journal_log,
                bytes_deserialized),
            SuccessExecutionResult());

  total_bytes_deserialized += bytes_deserialized;
  EXPECT_EQ(bytes_serialized, total_bytes_deserialized);
  EXPECT_EQ(deserialized_journal_log.type(), journal_log.type());

  bytes_buffer.bytes = make_shared<vector<Byte>>(kLogHeaderByteLength - 1);
  bytes_buffer.length = 0;
  bytes_buffer.capacity = kLogHeaderByteLength - 1;
  size_t new_bytes_serialized = 0;
  EXPECT_EQ(
      mock_journal_output_stream.SerializeLog(journal_stream_append_log_context,
                                              bytes_buffer,
                                              new_bytes_serialized),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE));

  EXPECT_EQ(new_bytes_serialized, 0);
  bytes_buffer.length += new_bytes_serialized;

  bytes_buffer.bytes = make_shared<vector<Byte>>(kLogHeaderByteLength + 1);
  bytes_buffer.length = 0;
  bytes_buffer.capacity = kLogHeaderByteLength + 1;
  new_bytes_serialized = 0;
  EXPECT_EQ(
      mock_journal_output_stream.SerializeLog(journal_stream_append_log_context,
                                              bytes_buffer,
                                              new_bytes_serialized),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE));

  EXPECT_EQ(new_bytes_serialized, kLogHeaderByteLength);
}

TEST(JournalOutputStreamTests, WriteJournalBlob) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockAsyncExecutor async_executor_mock;
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(async_executor_mock));

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(12345)};
  for (auto result : results) {
    BytesBuffer bytes_buffer;
    bytes_buffer.bytes = make_shared<vector<Byte>>(1000);
    bytes_buffer.capacity = 123;
    bytes_buffer.length = 456;
    MockBlobStorageClient mock_storage_client;
    mock_storage_client.put_blob_mock =
        [&](AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) {
          EXPECT_EQ(*put_blob_context.request->blob_name,
                    "partition_name/journal_00000000000000000001");
          EXPECT_EQ(*put_blob_context.request->bucket_name, "bucket_name");
          EXPECT_EQ(put_blob_context.request->buffer->bytes->size(), 1000);
          EXPECT_EQ(put_blob_context.request->buffer->capacity, 123);
          EXPECT_EQ(put_blob_context.request->buffer->length, 456);
          return result;
        };
    shared_ptr<BlobStorageClientInterface> storage_client =
        make_shared<MockBlobStorageClient>(move(mock_storage_client));

    MockJournalOutputStream mock_journal_output_stream(
        bucket_name, partition_name, async_executor, storage_client);

    auto callback = [](ExecutionResult&) {};
    EXPECT_EQ(
        mock_journal_output_stream.WriteJournalBlob(bytes_buffer, 1, callback),
        result);
  }
}

TEST(JournalOutputStreamTests, WriteEmptyJournalBlob) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockAsyncExecutor async_executor_mock;
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>(move(async_executor_mock));

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(12345)};
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>();
  bytes_buffer.capacity = 0;
  bytes_buffer.length = 0;
  MockBlobStorageClient mock_storage_client;
  mock_storage_client.put_blob_mock =
      [&](AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) {
        EXPECT_EQ(true, false);
        return SuccessExecutionResult();
      };
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalOutputStream mock_journal_output_stream(
      bucket_name, partition_name, async_executor, storage_client);

  auto callback = [](ExecutionResult& result) { EXPECT_SUCCESS(result); };
  EXPECT_SUCCESS(
      mock_journal_output_stream.WriteJournalBlob(bytes_buffer, 1, callback));
}

TEST(JournalOutputStreamTests, OnWriteJournalBlobCallback) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<BlobStorageClientInterface> storage_client;

  MockJournalOutputStream mock_journal_output_stream(
      bucket_name, partition_name, async_executor, storage_client);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(12345)};
  for (auto result : results) {
    atomic<bool> condition = false;
    auto callback = [&](ExecutionResult& execution_result) {
      EXPECT_THAT(execution_result, ResultIs(result));
      condition = true;
    };
    AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context;
    put_blob_context.result = result;
    mock_journal_output_stream.OnWriteJournalBlobCallback(0, callback,
                                                          put_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST(JournalOutputStreamTests, GetLastPersistedJournalId) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  shared_ptr<AsyncExecutorInterface> async_executor;
  shared_ptr<BlobStorageClientInterface> storage_client;

  MockJournalOutputStream mock_journal_output_stream(
      bucket_name, partition_name, async_executor, storage_client);

  for (JournalId i = 1; i < 100; ++i) {
    shared_ptr<bool> value;
    auto pair = make_pair(i, make_shared<bool>(false));
    mock_journal_output_stream.GetPersistedJournalIds().Insert(pair, value);
  }

  JournalId journal_id;
  auto execution_result =
      mock_journal_output_stream.GetLastPersistedJournalId(journal_id);
  EXPECT_THAT(
      execution_result,
      ResultIs(FailureExecutionResult(
          core::errors::SC_JOURNAL_SERVICE_NO_NEW_JOURNAL_ID_AVAILABLE)));

  vector<JournalId> keys;
  for (JournalId i = 50; i < 100; ++i) {
    shared_ptr<bool> value;
    mock_journal_output_stream.GetPersistedJournalIds().Find(i, value);
    *value = true;
    execution_result =
        mock_journal_output_stream.GetLastPersistedJournalId(journal_id);
    EXPECT_THAT(
        execution_result,
        ResultIs(FailureExecutionResult(
            core::errors::SC_JOURNAL_SERVICE_NO_NEW_JOURNAL_ID_AVAILABLE)));

    mock_journal_output_stream.GetPersistedJournalIds().Keys(keys);
    EXPECT_EQ(keys.size(), 99);
  }

  shared_ptr<bool> value;
  mock_journal_output_stream.GetPersistedJournalIds().Find(1, value);
  *value = true;

  for (int i = 0; i < 100; ++i) {
    execution_result =
        mock_journal_output_stream.GetLastPersistedJournalId(journal_id);
    EXPECT_SUCCESS(execution_result);
    EXPECT_EQ(journal_id, 1);

    mock_journal_output_stream.GetPersistedJournalIds().Keys(keys);
    EXPECT_EQ(keys.size(), 98);
  }

  mock_journal_output_stream.GetPersistedJournalIds().Find(13, value);
  *value = true;
  for (int i = 0; i < 100; ++i) {
    execution_result =
        mock_journal_output_stream.GetLastPersistedJournalId(journal_id);
    EXPECT_SUCCESS(execution_result);
    EXPECT_EQ(journal_id, 1);

    mock_journal_output_stream.GetPersistedJournalIds().Keys(keys);
    EXPECT_EQ(keys.size(), 98);
  }
  *value = false;

  auto current_size = keys.size();
  for (JournalId i = 2; i <= 50; ++i) {
    mock_journal_output_stream.GetPersistedJournalIds().Find(i, value);
    *value = true;
    current_size--;
    for (JournalId j = i + 1; j < 50; ++j) {
      execution_result =
          mock_journal_output_stream.GetLastPersistedJournalId(journal_id);
      EXPECT_SUCCESS(execution_result);
      EXPECT_EQ(journal_id, i);

      mock_journal_output_stream.GetPersistedJournalIds().Keys(keys);
      EXPECT_EQ(keys.size(), current_size);
    }
  }

  execution_result =
      mock_journal_output_stream.GetLastPersistedJournalId(journal_id);
  EXPECT_SUCCESS(execution_result);
  EXPECT_EQ(journal_id, 99);
  mock_journal_output_stream.GetPersistedJournalIds().Keys(keys);
  EXPECT_EQ(keys.size(), 0);
}
}  // namespace google::scp::core::test
