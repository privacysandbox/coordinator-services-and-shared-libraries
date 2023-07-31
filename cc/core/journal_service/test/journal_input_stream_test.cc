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

#include <atomic>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/common/serialization/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/journal_service/mock/mock_journal_input_stream.h"
#include "core/journal_service/src/error_codes.h"
#include "core/journal_service/src/journal_serialization.h"
#include "core/journal_service/src/proto/journal_service.pb.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_test_lib.h"

using google::scp::core::JournalLogStatus;
using google::scp::core::Timestamp;
using google::scp::core::blob_storage_provider::mock::MockBlobStorageClient;
using google::scp::core::common::Uuid;
using google::scp::core::journal_service::JournalLog;
using google::scp::core::journal_service::JournalSerialization;
using google::scp::core::journal_service::JournalStreamReadLogObject;
using google::scp::core::journal_service::JournalStreamReadLogRequest;
using google::scp::core::journal_service::JournalStreamReadLogResponse;
using google::scp::core::journal_service::mock::MockJournalInputStream;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::move;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::core::test {

class JournalInputStreamTests : public ::testing::Test {
 protected:
  JournalInputStreamTests() {
    auto bucket_name = make_shared<string>("bucket_name");
    auto partition_name = make_shared<string>("partition_name");
    mock_storage_client_ = make_shared<MockBlobStorageClient>();
    shared_ptr<BlobStorageClientInterface> storage_client_ =
        mock_storage_client_;
    mock_journal_input_stream_ = make_shared<MockJournalInputStream>(
        bucket_name, partition_name, storage_client_);
  }

  shared_ptr<MockBlobStorageClient> mock_storage_client_;
  shared_ptr<MockJournalInputStream> mock_journal_input_stream_;
};

TEST_F(JournalInputStreamTests, ReadLastCheckpointBlob) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(12345)};
  for (auto result : results) {
    mock_storage_client.get_blob_mock =
        [&](AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) {
          EXPECT_EQ(*get_blob_context.request->bucket_name, *bucket_name);
          EXPECT_EQ(*get_blob_context.request->blob_name,
                    string(*partition_name + "/last_checkpoint"));
          return result;
        };
    shared_ptr<BlobStorageClientInterface> storage_client =
        make_shared<MockBlobStorageClient>(move(mock_storage_client));
    MockJournalInputStream mock_journal_input_stream(
        bucket_name, partition_name, storage_client);

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    EXPECT_EQ(mock_journal_input_stream.ReadLastCheckpointBlob(
                  journal_stream_read_log_context),
              result);
  }
}

TEST_F(JournalInputStreamTests, OnReadLastCheckpointBlobCallbackBlobNotFound) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  // When the result is journal_logthing but success or blob not found
  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};
  for (auto result : results) {
    atomic<bool> condition(false);
    AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
    get_blob_context.result = result;
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_EQ(journal_stream_read_log_context.result, result);
          condition = true;
        };
    mock_journal_input_stream.OnReadLastCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, OnReadLastCheckpointListFails) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  // When the result is blob not found, but list result is not successful
  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
  get_blob_context.result = FailureExecutionResult(
      errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND);
  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};
  for (auto result : results) {
    atomic<bool> condition(false);
    mock_journal_input_stream.list_checkpoints_mock =
        [&](AsyncContext<journal_service::JournalStreamReadLogRequest,
                         journal_service::JournalStreamReadLogResponse>&,
            std::shared_ptr<Blob>&) { return result; };

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_EQ(journal_stream_read_log_context.result, result);
          condition = true;
        };
    mock_journal_input_stream.OnReadLastCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, OnReadLastCheckpointBlobCorrupted) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  // When the result is blob found, but the content is broken
  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
  get_blob_context.result = SuccessExecutionResult();

  atomic<bool> condition(false);
  vector<BytesBuffer> buffers(2);
  buffers[0].bytes = make_shared<vector<Byte>>(2);
  buffers[0].capacity = 2;
  buffers[0].length = 2;

  buffers[1].bytes = make_shared<vector<Byte>>(22);
  buffers[1].capacity = 22;
  buffers[1].length = 22;

  for (auto buffer : buffers) {
    get_blob_context.response = make_shared<GetBlobResponse>();
    get_blob_context.response->buffer = make_shared<BytesBuffer>(buffer);

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    journal_stream_read_log_context
        .callback = [&](AsyncContext<JournalStreamReadLogRequest,
                                     JournalStreamReadLogResponse>&
                            journal_stream_read_log_context) {
      if (buffer.capacity == 2) {
        EXPECT_THAT(journal_stream_read_log_context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_SERIALIZATION_BUFFER_NOT_READABLE)));
      } else {
        EXPECT_THAT(
            journal_stream_read_log_context.result,
            ResultIs(FailureExecutionResult(
                errors::
                    SC_JOURNAL_SERVICE_INPUT_STREAM_INVALID_LAST_CHECKPOINT)));
      }
      condition = true;
    };
    mock_journal_input_stream.OnReadLastCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, OnReadLastCheckpointBlobReadBlobFails) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  // When the result is blob found and the content looks good but reading the
  // checkpoint file immediately fails
  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
  get_blob_context.result = SuccessExecutionResult();

  journal_service::LastCheckpointMetadata last_checkpoint_metadata;
  last_checkpoint_metadata.set_last_checkpoint_id(1234);

  get_blob_context.response = make_shared<GetBlobResponse>();
  get_blob_context.response->buffer = make_shared<BytesBuffer>();
  get_blob_context.response->buffer->bytes = make_shared<vector<Byte>>(1000);
  get_blob_context.response->buffer->capacity = 1000;
  get_blob_context.response->buffer->length = 1000;

  size_t byte_serialized = 0;
  JournalSerialization::SerializeLastCheckpointMetadata(
      *get_blob_context.response->buffer, 0, last_checkpoint_metadata,
      byte_serialized);

  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    atomic<bool> condition(false);
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    mock_journal_input_stream.read_checkpoint_blob_mock =
        [&](AsyncContext<journal_service::JournalStreamReadLogRequest,
                         journal_service::JournalStreamReadLogResponse>&
                journal_stream_read_log_context,
            size_t checkpoint_id) {
          EXPECT_EQ(checkpoint_id,
                    last_checkpoint_metadata.last_checkpoint_id());
          EXPECT_EQ(mock_journal_input_stream.GetLastCheckpointId(),
                    checkpoint_id);
          return result;
        };

    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_EQ(journal_stream_read_log_context.result, result);
          condition = true;
        };
    mock_journal_input_stream.OnReadLastCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, ReadCheckpointBlob) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockBlobStorageClient mock_storage_client;
  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    mock_storage_client.get_blob_mock =
        [&](AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) {
          EXPECT_EQ(*get_blob_context.request->bucket_name, *bucket_name);
          EXPECT_EQ(
              *get_blob_context.request->blob_name,
              string(*partition_name + "/checkpoint_00000000000000000100"));
          return result;
        };
    shared_ptr<BlobStorageClientInterface> storage_client =
        make_shared<MockBlobStorageClient>(move(mock_storage_client));

    MockJournalInputStream mock_journal_input_stream(
        bucket_name, partition_name, storage_client);

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    EXPECT_EQ(mock_journal_input_stream.ReadCheckpointBlob(
                  journal_stream_read_log_context, 100 /* checkpoint_id */),
              result);
  }
}

TEST_F(JournalInputStreamTests, OnReadCheckpointBlobCallback) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  // When the result is journal_logthing but success
  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};
  for (auto result : results) {
    atomic<bool> condition(false);
    AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
    get_blob_context.result = result;
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_EQ(journal_stream_read_log_context.result, result);
          condition = true;
        };
    mock_journal_input_stream.OnReadCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, OnReadCheckpointBlobCorruptedBlob) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  // When the result is blob found, but the content is broken
  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
  get_blob_context.result = SuccessExecutionResult();

  atomic<bool> condition(false);
  vector<BytesBuffer> buffers(2);
  buffers[0].bytes = make_shared<vector<Byte>>(2);
  buffers[0].capacity = 2;
  buffers[0].length = 2;

  buffers[1].bytes = make_shared<vector<Byte>>(22);
  buffers[1].capacity = 22;
  buffers[1].length = 22;

  for (auto buffer : buffers) {
    get_blob_context.response = make_shared<GetBlobResponse>();
    get_blob_context.response->buffer = make_shared<BytesBuffer>(buffer);

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          if (buffer.capacity == 2) {
            EXPECT_EQ(journal_stream_read_log_context.result,
                      FailureExecutionResult(
                          errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
          } else {
            EXPECT_EQ(
                journal_stream_read_log_context.result,
                FailureExecutionResult(
                    errors::SC_JOURNAL_SERVICE_MAGIC_NUMBER_NOT_MATCHING));
          }
          condition = true;
        };
    mock_journal_input_stream.OnReadCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, OnReadCheckpointBlobListBlobsFail) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  // When the result is blob found and the content looks good but list
  // the journal blobs immediately fails
  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
  get_blob_context.result = SuccessExecutionResult();

  journal_service::CheckpointMetadata checkpoint_metadata;
  checkpoint_metadata.set_last_processed_journal_id(1234);

  get_blob_context.response = make_shared<GetBlobResponse>();
  get_blob_context.response->buffer = make_shared<BytesBuffer>();
  get_blob_context.response->buffer->bytes = make_shared<vector<Byte>>(1000);
  get_blob_context.response->buffer->capacity = 1000;

  size_t byte_serialized = 0;
  JournalSerialization::SerializeCheckpointMetadata(
      *get_blob_context.response->buffer, 0, checkpoint_metadata,
      byte_serialized);
  get_blob_context.response->buffer->length = byte_serialized;

  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(12345)};

  for (auto result : results) {
    MockJournalInputStream mock_journal_input_stream(
        bucket_name, partition_name, storage_client);
    atomic<bool> condition(false);
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    mock_journal_input_stream.list_journals_mock =
        [&](AsyncContext<journal_service::JournalStreamReadLogRequest,
                         journal_service::JournalStreamReadLogResponse>&,
            std::shared_ptr<Blob>& start_name) {
          EXPECT_EQ(mock_journal_input_stream.GetLastProcessedJournalId(),
                    checkpoint_metadata.last_processed_journal_id());
          auto journal_buffers = mock_journal_input_stream.GetJournalBuffers();
          EXPECT_EQ(journal_buffers.size(), 1);
          EXPECT_EQ(*start_name->blob_name,
                    "partition_name/journal_00000000000000001234");
          return result;
        };

    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_EQ(journal_stream_read_log_context.result, result);
          condition = true;
        };
    mock_journal_input_stream.OnReadCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, ListCheckpoints) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(12345)};
  for (auto result : results) {
    mock_storage_client.list_blobs_mock =
        [&](AsyncContext<ListBlobsRequest, ListBlobsResponse>&
                list_blobs_context) {
          EXPECT_EQ(*list_blobs_context.request->bucket_name, *bucket_name);
          EXPECT_EQ(*list_blobs_context.request->blob_name,
                    string(*partition_name + "/checkpoint_"));

          if (result.status == ExecutionStatus::Failure) {
            EXPECT_EQ(*list_blobs_context.request->marker, string("test"));
          } else {
            EXPECT_EQ(list_blobs_context.request->marker, nullptr);
          }
          return result;
        };
    shared_ptr<BlobStorageClientInterface> storage_client =
        make_shared<MockBlobStorageClient>(move(mock_storage_client));

    MockJournalInputStream mock_journal_input_stream(
        bucket_name, partition_name, storage_client);

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    auto blob = make_shared<Blob>();

    if (result.status == ExecutionStatus::Failure) {
      blob->blob_name = make_shared<string>("test");
    }

    EXPECT_EQ(mock_journal_input_stream.ListCheckpoints(
                  journal_stream_read_log_context, blob),
              result);
  }
}

TEST_F(JournalInputStreamTests, OnListCheckpointsCallback) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  // When the result is journal_logthing but success
  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};
  for (auto result : results) {
    atomic<bool> condition(false);
    AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
    list_blobs_context.result = result;
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_EQ(journal_stream_read_log_context.result, result);
          condition = true;
        };
    mock_journal_input_stream.OnListCheckpointsCallback(
        journal_stream_read_log_context, list_blobs_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, OnListCheckpointsCallbackListFails) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  // When the result is success but there are no checkpoint blobs
  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};
  for (auto result : results) {
    atomic<bool> condition(false);
    AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
    list_blobs_context.result = SuccessExecutionResult();
    list_blobs_context.response = make_shared<ListBlobsResponse>();
    list_blobs_context.response->blobs = make_shared<vector<Blob>>();
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;

    mock_journal_input_stream.list_journals_mock =
        [&](AsyncContext<journal_service::JournalStreamReadLogRequest,
                         journal_service::JournalStreamReadLogResponse>&
                context,
            std::shared_ptr<Blob>& start_from) {
          // Because there were no checkpoints in the listing, all the
          // journals are read, i.e. start_from will be nullptr.
          EXPECT_EQ(start_from, nullptr);
          return result;
        };

    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_EQ(journal_stream_read_log_context.result, result);
          condition = true;
        };

    mock_journal_input_stream.OnListCheckpointsCallback(
        journal_stream_read_log_context, list_blobs_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, OnListCheckpointsCallbackWrongBlobNames) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  // When the result is success but there are checkpoint blobs with wrong name
  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};
  atomic<bool> condition(false);
  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.result = SuccessExecutionResult();
  list_blobs_context.response = make_shared<ListBlobsResponse>();
  list_blobs_context.response->blobs = make_shared<vector<Blob>>();
  Blob blob;
  blob.blob_name = make_shared<string>("checkpoint_12312_ddd");
  list_blobs_context.response->blobs->push_back(blob);
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;

  journal_stream_read_log_context.callback =
      [&](AsyncContext<JournalStreamReadLogRequest,
                       JournalStreamReadLogResponse>&
              journal_stream_read_log_context) {
        EXPECT_EQ(journal_stream_read_log_context.result,
                  FailureExecutionResult(
                      errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));
        condition = true;
      };
  mock_journal_input_stream.OnListCheckpointsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(JournalInputStreamTests, OnListCheckpointsCallbackInvalidIndex) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  mock_storage_client.list_blobs_mock =
      [](AsyncContext<ListBlobsRequest, ListBlobsResponse>& list_context) {
        EXPECT_NE(list_context.request->marker, nullptr);
        EXPECT_EQ(*list_context.request->marker,
                  "partition_name/checkpoint_12315");
        list_context.result = SuccessExecutionResult();
        list_context.response = make_shared<ListBlobsResponse>();
        list_context.response->blobs = make_shared<vector<Blob>>();
        list_context.Finish();
        return SuccessExecutionResult();
      };

  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  atomic<bool> condition(false);
  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.result = SuccessExecutionResult();
  list_blobs_context.response = make_shared<ListBlobsResponse>();
  list_blobs_context.response->blobs = make_shared<vector<Blob>>();
  Blob blob;
  blob.blob_name = make_shared<string>("partition_name/checkpoint_12312");
  list_blobs_context.response->blobs->push_back(blob);
  Blob blob1;
  blob1.blob_name = make_shared<string>("partition_name/checkpoint_12315");
  list_blobs_context.response->blobs->push_back(blob1);

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;

  mock_journal_input_stream.read_checkpoint_blob_mock =
      [&](AsyncContext<journal_service::JournalStreamReadLogRequest,
                       journal_service::JournalStreamReadLogResponse>&,
          size_t checkpoint_index) {
        EXPECT_EQ(checkpoint_index, 12315);
        EXPECT_EQ(mock_journal_input_stream.GetLastCheckpointId(), 12315);
        return FailureExecutionResult(123);
      };

  journal_stream_read_log_context.callback =
      [&](AsyncContext<JournalStreamReadLogRequest,
                       JournalStreamReadLogResponse>&
              journal_stream_read_log_context) {
        EXPECT_EQ(journal_stream_read_log_context.result,
                  FailureExecutionResult(123));
        condition = true;
      };

  mock_journal_input_stream.OnListCheckpointsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(JournalInputStreamTests, OnListCheckpointsCallbackWithMarker) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};

  for (auto result : results) {
    atomic<bool> condition(false);
    AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
    list_blobs_context.result = SuccessExecutionResult();
    list_blobs_context.response = make_shared<ListBlobsResponse>();
    list_blobs_context.response->blobs = make_shared<vector<Blob>>();
    list_blobs_context.response->next_marker = make_shared<Blob>();
    list_blobs_context.response->next_marker->bucket_name = bucket_name;
    list_blobs_context.response->next_marker->blob_name =
        make_shared<string>("marker");

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;

    mock_journal_input_stream.list_checkpoints_mock =
        [&](AsyncContext<journal_service::JournalStreamReadLogRequest,
                         journal_service::JournalStreamReadLogResponse>&,
            std::shared_ptr<Blob>& start_from) {
          EXPECT_EQ(*start_from->blob_name, "marker");
          EXPECT_EQ(*start_from->bucket_name, *bucket_name);
          return result;
        };

    mock_journal_input_stream.read_checkpoint_blob_mock =
        [&](AsyncContext<journal_service::JournalStreamReadLogRequest,
                         journal_service::JournalStreamReadLogResponse>&,
            size_t checkpoint_index) {
          EXPECT_EQ(true, false);
          return FailureExecutionResult(123);
        };

    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_EQ(journal_stream_read_log_context.result, result);
          condition = true;
        };
    mock_journal_input_stream.OnListCheckpointsCallback(
        journal_stream_read_log_context, list_blobs_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, ListJournals) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(12345)};
  for (auto result : results) {
    mock_storage_client.list_blobs_mock =
        [&](AsyncContext<ListBlobsRequest, ListBlobsResponse>&
                list_blobs_context) {
          EXPECT_EQ(*list_blobs_context.request->bucket_name, *bucket_name);
          EXPECT_EQ(*list_blobs_context.request->blob_name,
                    string(*partition_name + "/journal_"));

          if (result.status == ExecutionStatus::Failure) {
            EXPECT_EQ(*list_blobs_context.request->marker, string("test"));
          } else {
            EXPECT_EQ(list_blobs_context.request->marker, nullptr);
          }
          return result;
        };
    shared_ptr<BlobStorageClientInterface> storage_client =
        make_shared<MockBlobStorageClient>(move(mock_storage_client));

    MockJournalInputStream mock_journal_input_stream(
        bucket_name, partition_name, storage_client);

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    auto blob = make_shared<Blob>();

    if (result.status == ExecutionStatus::Failure) {
      blob->blob_name = make_shared<string>("test");
    }

    EXPECT_EQ(mock_journal_input_stream.ListJournals(
                  journal_stream_read_log_context, blob),
              result);
  }
}

TEST_F(JournalInputStreamTests, OnListJournalsCallback) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  // When the result is journal_logthing but success
  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};
  for (auto result : results) {
    atomic<bool> condition(false);
    AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
    list_blobs_context.result = result;
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_EQ(journal_stream_read_log_context.result, result);
          condition = true;
        };
    mock_journal_input_stream.OnListJournalsCallback(
        journal_stream_read_log_context, list_blobs_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, OnListJournalsCallbackNoJournalBlobs) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);

  atomic<bool> condition(false);
  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.result = SuccessExecutionResult();
  list_blobs_context.response = make_shared<ListBlobsResponse>();
  list_blobs_context.response->blobs = make_shared<vector<Blob>>();
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;

  journal_stream_read_log_context.callback =
      [&](AsyncContext<JournalStreamReadLogRequest,
                       JournalStreamReadLogResponse>&
              journal_stream_read_log_context) {
        EXPECT_EQ(journal_stream_read_log_context.result,
                  SuccessExecutionResult());
        condition = true;
      };

  mock_journal_input_stream.OnListJournalsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(JournalInputStreamTests, OnListJournalsCallbackWrongBlobNames) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  // When the result is success but there are checkpoint blobs with wrong name
  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};
  atomic<bool> condition(false);
  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.result = SuccessExecutionResult();
  list_blobs_context.response = make_shared<ListBlobsResponse>();
  list_blobs_context.response->blobs = make_shared<vector<Blob>>();
  Blob blob;
  blob.blob_name = make_shared<string>("journal_12312_ddd");
  list_blobs_context.response->blobs->push_back(blob);
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;

  journal_stream_read_log_context.callback =
      [&](AsyncContext<JournalStreamReadLogRequest,
                       JournalStreamReadLogResponse>&
              journal_stream_read_log_context) {
        EXPECT_EQ(journal_stream_read_log_context.result,
                  FailureExecutionResult(
                      errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));
        condition = true;
      };
  mock_journal_input_stream.OnListJournalsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(JournalInputStreamTests, OnListJournalsCallbackProperListing) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  mock_storage_client.list_blobs_mock =
      [](AsyncContext<ListBlobsRequest, ListBlobsResponse>& list_context) {
        EXPECT_NE(list_context.request->marker, nullptr);
        EXPECT_EQ(*list_context.request->marker,
                  "partition_name/journal_12315");
        list_context.result = SuccessExecutionResult();
        list_context.response = make_shared<ListBlobsResponse>();
        list_context.response->blobs = make_shared<vector<Blob>>();
        list_context.Finish();
        return SuccessExecutionResult();
      };

  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  atomic<bool> condition(false);
  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.result = SuccessExecutionResult();
  list_blobs_context.response = make_shared<ListBlobsResponse>();
  list_blobs_context.response->blobs = make_shared<vector<Blob>>();
  Blob blob;
  blob.blob_name = make_shared<string>("partition_name/journal_12312");
  list_blobs_context.response->blobs->push_back(blob);
  Blob blob1;
  blob1.blob_name = make_shared<string>("partition_name/journal_12333312315");
  list_blobs_context.response->blobs->push_back(blob1);
  Blob blob2;
  blob2.blob_name = make_shared<string>("partition_name/journal_12315");
  list_blobs_context.response->blobs->push_back(blob2);

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  journal_stream_read_log_context.request =
      make_shared<JournalStreamReadLogRequest>();

  mock_journal_input_stream.read_journal_blobs_mock =
      [&](AsyncContext<journal_service::JournalStreamReadLogRequest,
                       journal_service::JournalStreamReadLogResponse>&,
          vector<uint64_t>& journal_ids) {
        EXPECT_EQ(journal_ids.size(), 3);
        EXPECT_EQ(journal_ids[0], 12312);
        EXPECT_EQ(journal_ids[1], 12333312315);
        EXPECT_EQ(journal_ids[2], 12315);
        EXPECT_EQ(mock_journal_input_stream.GetLastProcessedJournalId(),
                  12333312315);
        return FailureExecutionResult(123);
      };

  journal_stream_read_log_context.callback =
      [&](AsyncContext<JournalStreamReadLogRequest,
                       JournalStreamReadLogResponse>&
              journal_stream_read_log_context) {
        EXPECT_EQ(journal_stream_read_log_context.result,
                  FailureExecutionResult(123));
        condition = true;
      };
  mock_journal_input_stream.OnListJournalsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(JournalInputStreamTests,
       OnListJournalsCallbackProperListingWithMaxLoaded) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  mock_storage_client.list_blobs_mock =
      [](AsyncContext<ListBlobsRequest, ListBlobsResponse>& list_context) {
        EXPECT_EQ(true, false);
        return SuccessExecutionResult();
      };

  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  atomic<bool> condition(false);
  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.result = SuccessExecutionResult();
  list_blobs_context.response = make_shared<ListBlobsResponse>();
  list_blobs_context.response->blobs = make_shared<vector<Blob>>();
  Blob blob;
  blob.blob_name = make_shared<string>("partition_name/journal_12312");
  list_blobs_context.response->blobs->push_back(blob);
  Blob blob1;
  blob1.blob_name = make_shared<string>("partition_name/journal_12345");
  list_blobs_context.response->blobs->push_back(blob1);
  Blob blob2;
  blob2.blob_name = make_shared<string>("partition_name/journal_12346");
  list_blobs_context.response->blobs->push_back(blob2);

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  journal_stream_read_log_context.request =
      make_shared<JournalStreamReadLogRequest>();
  journal_stream_read_log_context.request->max_journal_id_to_process = 12345;

  mock_journal_input_stream.read_journal_blobs_mock =
      [&](AsyncContext<journal_service::JournalStreamReadLogRequest,
                       journal_service::JournalStreamReadLogResponse>&,
          vector<uint64_t>& journal_ids) {
        EXPECT_EQ(journal_ids.size(), 2);
        EXPECT_EQ(journal_ids[0], 12312);
        EXPECT_EQ(journal_ids[1], 12345);
        EXPECT_EQ(mock_journal_input_stream.GetLastProcessedJournalId(), 12345);
        return FailureExecutionResult(123);
      };

  journal_stream_read_log_context.callback =
      [&](AsyncContext<JournalStreamReadLogRequest,
                       JournalStreamReadLogResponse>&
              journal_stream_read_log_context) {
        EXPECT_EQ(journal_stream_read_log_context.result,
                  FailureExecutionResult(123));
        condition = true;
      };
  mock_journal_input_stream.OnListJournalsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(JournalInputStreamTests,
       OnListJournalsCallbackProperListingWithMaxRecoverFiles) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  mock_storage_client.list_blobs_mock =
      [](AsyncContext<ListBlobsRequest, ListBlobsResponse>& list_context) {
        EXPECT_EQ(true, false);
        return SuccessExecutionResult();
      };

  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  atomic<bool> condition(false);
  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.result = SuccessExecutionResult();
  list_blobs_context.response = make_shared<ListBlobsResponse>();
  list_blobs_context.response->blobs = make_shared<vector<Blob>>();
  Blob blob;
  blob.blob_name = make_shared<string>("partition_name/journal_12312");
  list_blobs_context.response->blobs->push_back(blob);
  Blob blob1;
  blob1.blob_name = make_shared<string>("partition_name/journal_12345");
  list_blobs_context.response->blobs->push_back(blob1);
  Blob blob2;
  blob2.blob_name = make_shared<string>("partition_name/journal_12346");
  list_blobs_context.response->blobs->push_back(blob2);

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  journal_stream_read_log_context.request =
      make_shared<JournalStreamReadLogRequest>();
  journal_stream_read_log_context.request->max_number_of_journals_to_process =
      2;

  mock_journal_input_stream.read_journal_blobs_mock =
      [&](AsyncContext<journal_service::JournalStreamReadLogRequest,
                       journal_service::JournalStreamReadLogResponse>&,
          vector<uint64_t>& journal_ids) {
        EXPECT_EQ(journal_ids.size(), 2);
        EXPECT_EQ(journal_ids[0], 12312);
        EXPECT_EQ(journal_ids[1], 12345);
        EXPECT_EQ(mock_journal_input_stream.GetLastProcessedJournalId(), 12345);
        return FailureExecutionResult(123);
      };

  journal_stream_read_log_context.callback =
      [&](AsyncContext<JournalStreamReadLogRequest,
                       JournalStreamReadLogResponse>&
              journal_stream_read_log_context) {
        EXPECT_EQ(journal_stream_read_log_context.result,
                  FailureExecutionResult(123));
        condition = true;
      };
  mock_journal_input_stream.OnListJournalsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(JournalInputStreamTests, OnListJournalsCallbackWithMarker) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};

  for (auto result : results) {
    atomic<bool> condition(false);
    AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
    list_blobs_context.result = SuccessExecutionResult();
    list_blobs_context.response = make_shared<ListBlobsResponse>();
    list_blobs_context.response->blobs = make_shared<vector<Blob>>();
    list_blobs_context.response->next_marker = make_shared<Blob>();
    list_blobs_context.response->next_marker->bucket_name = bucket_name;
    list_blobs_context.response->next_marker->blob_name =
        make_shared<string>("marker");

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;

    mock_journal_input_stream.list_journals_mock =
        [&](AsyncContext<journal_service::JournalStreamReadLogRequest,
                         journal_service::JournalStreamReadLogResponse>&,
            std::shared_ptr<Blob>& start_from) {
          EXPECT_EQ(*start_from->blob_name, "marker");
          EXPECT_EQ(*start_from->bucket_name, *bucket_name);
          return result;
        };

    mock_journal_input_stream.read_journal_blobs_mock =
        [&](AsyncContext<journal_service::JournalStreamReadLogRequest,
                         journal_service::JournalStreamReadLogResponse>&,
            vector<uint64_t>& journal_ids) {
          EXPECT_EQ(true, false);
          return FailureExecutionResult(123);
        };

    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_EQ(journal_stream_read_log_context.result, result);
          condition = true;
        };
    mock_journal_input_stream.OnListJournalsCallback(
        journal_stream_read_log_context, list_blobs_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, ReadJournalBlobsWithEmptyBlobsList) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  journal_stream_read_log_context.callback = [&](auto&) {};

  vector<uint64_t> journal_ids;
  EXPECT_THAT(mock_journal_input_stream.ReadJournalBlobs(
                  journal_stream_read_log_context, journal_ids),
              ResultIs(FailureExecutionResult(
                  errors::SC_JOURNAL_SERVICE_INPUT_STREAM_INVALID_LISTING)));
}

TEST_F(JournalInputStreamTests, ReadJournalBlobsProperly) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  // When the result is journal_logthing but success
  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};
  for (auto result : results) {
    MockJournalInputStream mock_journal_input_stream(
        bucket_name, partition_name, storage_client);
    vector<size_t> journal_id_dispatched;
    vector<size_t> buffer_index_dispatched;

    mock_journal_input_stream.read_journal_blob_mock =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&,
            size_t journal_id, size_t buffer_index) {
          journal_id_dispatched.push_back(journal_id);
          buffer_index_dispatched.push_back(buffer_index);
          return result;
        };

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    journal_stream_read_log_context.callback = [&](auto&) {};

    vector<uint64_t> journal_ids = {100, 10, 24};
    EXPECT_EQ(mock_journal_input_stream.ReadJournalBlobs(
                  journal_stream_read_log_context, journal_ids),
              result);

    EXPECT_EQ(mock_journal_input_stream.GetJournalBuffers().size(), 3);
    EXPECT_EQ(journal_id_dispatched.size(), 3);
    EXPECT_EQ(journal_id_dispatched[0], 10);
    EXPECT_EQ(journal_id_dispatched[1], 24);
    EXPECT_EQ(journal_id_dispatched[2], 100);

    EXPECT_EQ(buffer_index_dispatched[0], 0);
    EXPECT_EQ(buffer_index_dispatched[1], 1);
    EXPECT_EQ(buffer_index_dispatched[2], 2);
  }
}

TEST_F(JournalInputStreamTests, ReadJournalBlobsFailedToSchedule) {
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  journal_stream_read_log_context.callback = [&](auto&) {};

  // Set up the mock to succeed the fail for the journal with id 100.
  mock_journal_input_stream_->read_journal_blob_mock =
      [&](AsyncContext<JournalStreamReadLogRequest,
                       JournalStreamReadLogResponse>&,
          size_t journal_id, size_t buffer_index) {
        ExecutionResult result = SuccessExecutionResult();
        if (journal_id == 100) {
          result = FailureExecutionResult(1234);
        }
        return result;
      };

  vector<uint64_t> journal_ids = {100, 10, 24};
  // This function call should not return a Failure as there are two more
  // callbacks to be recieved for the journals 10 and 24 and the last one
  // would finish the context.
  EXPECT_THAT(mock_journal_input_stream_->ReadJournalBlobs(
                  journal_stream_read_log_context, journal_ids),
              ResultIs(SuccessExecutionResult()));
}

TEST_F(JournalInputStreamTests, ReadJournalBlob) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(12345)};
  for (auto result : results) {
    mock_storage_client.get_blob_mock =
        [&](AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) {
          EXPECT_EQ(*get_blob_context.request->bucket_name, *bucket_name);
          EXPECT_EQ(*get_blob_context.request->blob_name,
                    string(*partition_name + "/journal_00000000000000000100"));
          return result;
        };
    shared_ptr<BlobStorageClientInterface> storage_client =
        make_shared<MockBlobStorageClient>(move(mock_storage_client));

    MockJournalInputStream mock_journal_input_stream(
        bucket_name, partition_name, storage_client);

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    EXPECT_EQ(mock_journal_input_stream.ReadJournalBlob(
                  journal_stream_read_log_context, 100 /* journal_id */,
                  10 /* buffer_index */),
              result);
  }
}

TEST_F(JournalInputStreamTests, OnReadJournalBlobCallback) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  // When the result is journal_logthing but success
  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};
  for (auto result : results) {
    MockJournalInputStream mock_journal_input_stream(
        bucket_name, partition_name, storage_client);
    atomic<bool> condition(false);
    AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
    get_blob_context.result = result;
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_EQ(journal_stream_read_log_context.result,
                    FailureExecutionResult(1234));
          condition = true;
        };
    mock_journal_input_stream.GetTotalJournalsToRead() = 1;
    mock_journal_input_stream.OnReadJournalBlobCallback(
        journal_stream_read_log_context, get_blob_context, 1);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_F(JournalInputStreamTests, OnReadJournalBlobCallbackDifferentBuffers) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);

  // When the result is journal_logthing but success
  vector<ExecutionResult> results = {FailureExecutionResult(1234),
                                     RetryExecutionResult(1234)};
  for (auto result : results) {
    AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
    get_blob_context.result = SuccessExecutionResult();
    get_blob_context.response = make_shared<GetBlobResponse>();
    get_blob_context.response->buffer = make_shared<BytesBuffer>();
    get_blob_context.response->buffer->bytes = make_shared<vector<Byte>>(1);
    get_blob_context.response->buffer->length = 100;
    get_blob_context.response->buffer->capacity = 200;

    mock_journal_input_stream.process_loaded_journals_mock =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&) { return result; };

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    journal_stream_read_log_context
        .callback = [&](AsyncContext<JournalStreamReadLogRequest,
                                     JournalStreamReadLogResponse>&
                            journal_stream_read_log_context) {
      EXPECT_EQ(mock_journal_input_stream.GetJournalBuffers()[0].bytes->size(),
                1);
      EXPECT_EQ(mock_journal_input_stream.GetJournalBuffers()[0].length, 100);
      EXPECT_EQ(mock_journal_input_stream.GetJournalBuffers()[0].capacity, 200);
    };

    mock_journal_input_stream.GetTotalJournalsToRead() = 1;
    mock_journal_input_stream.GetJournalBuffers().push_back(BytesBuffer());
    mock_journal_input_stream.OnReadJournalBlobCallback(
        journal_stream_read_log_context, get_blob_context, 0);
  }
}

BytesBuffer GenerateLogBytes(size_t count, set<Uuid>& completed_logs,
                             vector<Timestamp>& timestamps,
                             vector<Uuid>& component_ids, vector<Uuid>& log_ids,
                             vector<JournalLog>& journal_logs) {
  BytesBuffer bytes_buffer_0;
  bytes_buffer_0.bytes = make_shared<vector<Byte>>(10240000);
  bytes_buffer_0.capacity = 10240000;

  for (size_t i = 0; i < count; ++i) {
    Uuid component_uuid_0 = Uuid::GenerateUuid();
    Uuid log_uuid_0 = Uuid::GenerateUuid();
    Timestamp timestamp = 12341231;
    log_ids.push_back(log_uuid_0);
    component_ids.push_back(component_uuid_0);
    size_t bytes_serialized = 0;
    timestamps.push_back(timestamp);
    EXPECT_EQ(JournalSerialization::SerializeLogHeader(
                  bytes_buffer_0, bytes_buffer_0.length, timestamp,
                  JournalLogStatus::Log, component_uuid_0, log_uuid_0,
                  bytes_serialized),
              SuccessExecutionResult());

    bytes_buffer_0.length += bytes_serialized;

    JournalLog journal_log;
    journal_log.set_type(i);
    journal_logs.push_back(journal_log);
    bytes_serialized = 0;
    EXPECT_EQ(JournalSerialization::SerializeJournalLog(
                  bytes_buffer_0, bytes_buffer_0.length, journal_log,
                  bytes_serialized),
              SuccessExecutionResult());

    bytes_buffer_0.length += bytes_serialized;
  }
  return bytes_buffer_0;
}

TEST_F(JournalInputStreamTests, ProcessLoadedJournals) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  mock_journal_input_stream.process_next_journal_log_mock =
      [](Timestamp&, JournalLogStatus&, Uuid&, Uuid&, JournalLog&, JournalId&) {
        return FailureExecutionResult(1234);
      };
  EXPECT_EQ(mock_journal_input_stream.ProcessLoadedJournals(
                journal_stream_read_log_context),
            FailureExecutionResult(1234));
}

TEST_F(JournalInputStreamTests, ProcessLoadedJournalsProperly) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);

  atomic<bool> condition(false);
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  journal_stream_read_log_context.callback =
      [&](AsyncContext<JournalStreamReadLogRequest,
                       JournalStreamReadLogResponse>&
              journal_stream_read_log_context) {
        EXPECT_EQ(journal_stream_read_log_context.result,
                  SuccessExecutionResult());
        condition = true;
      };

  mock_journal_input_stream.process_next_journal_log_mock =
      [](Timestamp&, JournalLogStatus&, Uuid&, Uuid&, JournalLog&, JournalId&) {
        return SuccessExecutionResult();
      };
  EXPECT_EQ(mock_journal_input_stream.ProcessLoadedJournals(
                journal_stream_read_log_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(JournalInputStreamTests, ProcessLoadedJournalsSerializationFailure) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);

  BytesBuffer buffer;
  buffer.bytes = make_shared<vector<Byte>>();
  buffer.capacity = 0;
  buffer.length = 1;
  mock_journal_input_stream.GetJournalBuffers().push_back(buffer);
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  EXPECT_EQ(
      mock_journal_input_stream.ProcessLoadedJournals(
          journal_stream_read_log_context),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
}

TEST_F(JournalInputStreamTests, ProcessLoadedJournalsSerializationFailure2) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);

  BytesBuffer buffer(12);
  buffer.length = 12;
  mock_journal_input_stream.GetJournalBuffers().push_back(buffer);
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  EXPECT_EQ(
      mock_journal_input_stream.ProcessLoadedJournals(
          journal_stream_read_log_context),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
  EXPECT_EQ(mock_journal_input_stream.GetJournalsLoaded(), false);
}

TEST_F(JournalInputStreamTests, ProcessNextJournalLog) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  Timestamp timestamp;
  JournalLogStatus journal_log_status;
  JournalLog journal_log;
  Uuid component_id;
  Uuid log_id;
  JournalId journal_id;
  EXPECT_EQ(
      mock_journal_input_stream.ProcessNextJournalLog(
          timestamp, journal_log_status, component_id, log_id, journal_log,
          journal_id),
      FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN));
}

TEST_F(JournalInputStreamTests, ProcessNextJournalLogProperly) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);
  BytesBuffer bytes_buffer;
  mock_journal_input_stream.GetJournalBuffers().push_back(bytes_buffer);
  mock_journal_input_stream.GetCurrentBufferOffset()++;

  JournalLogStatus journal_log_status;
  JournalLog journal_log;
  Uuid component_id;
  Uuid log_id;
  Timestamp timestamp;
  JournalId journal_id;
  EXPECT_EQ(
      mock_journal_input_stream.ProcessNextJournalLog(
          timestamp, journal_log_status, component_id, log_id, journal_log,
          journal_id),
      FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN));
  EXPECT_EQ(mock_journal_input_stream.GetCurrentBufferIndex(), 1);
  EXPECT_EQ(mock_journal_input_stream.GetCurrentBufferOffset(), 0);
}

TEST_F(JournalInputStreamTests, ProcessNextJournalLogSerializeAndDeserialize) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);

  vector<size_t> counts = {0, 100};
  vector<JournalLog> pending_journal_logs;
  vector<Uuid> pending_journal_log_uuids;
  vector<Uuid> pending_journal_component_uuids;
  vector<Timestamp> pending_journal_timestamps;

  auto& journal_ids = mock_journal_input_stream.GetJournalIds();
  auto& journal_buffers = mock_journal_input_stream.GetJournalBuffers();
  for (size_t i = 0; i < counts.size(); ++i) {
    set<Uuid> completed_logs;
    vector<Uuid> log_ids;
    vector<JournalLog> journal_logs;
    vector<Timestamp> timestamps;
    vector<Uuid> component_ids;

    journal_ids.push_back(12341234);
    journal_buffers.push_back(GenerateLogBytes(counts[i], completed_logs,
                                               timestamps, component_ids,
                                               log_ids, journal_logs));

    for (size_t j = 0; j < journal_logs.size(); ++j) {
      pending_journal_logs.push_back(journal_logs[j]);
      pending_journal_log_uuids.push_back(log_ids[j]);
      pending_journal_component_uuids.push_back(component_ids[j]);
      pending_journal_timestamps.push_back(timestamps[j]);
    }
  }

  size_t index = 0;
  while (true) {
    JournalLogStatus journal_log_status;
    JournalLog journal_log;
    Uuid component_id;
    Uuid log_id;
    Timestamp timestamp;
    JournalId journal_id;
    auto execution_result = mock_journal_input_stream.ProcessNextJournalLog(
        timestamp, journal_log_status, component_id, log_id, journal_log,
        journal_id);
    if (!execution_result.Successful()) {
      EXPECT_EQ(
          execution_result,
          FailureExecutionResult(
              errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN));
      break;
    }

    EXPECT_EQ(journal_id, 12341234);
    EXPECT_EQ(pending_journal_logs[index].type(), journal_log.type());
    EXPECT_EQ(pending_journal_log_uuids[index], log_id);
    EXPECT_EQ(pending_journal_component_uuids[index], component_id);
    EXPECT_EQ(pending_journal_timestamps[index], timestamp);
    index++;
  }

  EXPECT_EQ(index, pending_journal_logs.size());
}

TEST_F(JournalInputStreamTests, ReadJournalLogBatch) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(bucket_name, partition_name,
                                                   storage_client);

  vector<size_t> counts = {0, 100};
  vector<JournalLog> pending_journal_logs;
  vector<Uuid> pending_journal_log_uuids;
  vector<Uuid> pending_journal_component_uuids;
  vector<Timestamp> pending_journal_timestamps;

  auto& journal_ids = mock_journal_input_stream.GetJournalIds();
  auto& journal_buffers = mock_journal_input_stream.GetJournalBuffers();
  for (size_t i = 0; i < counts.size(); ++i) {
    set<Uuid> completed_logs;
    vector<Uuid> log_ids;
    vector<JournalLog> journal_logs;
    vector<Timestamp> timestamps;
    vector<Uuid> component_ids;

    journal_ids.push_back(12344321);
    journal_buffers.push_back(GenerateLogBytes(counts[i], completed_logs,
                                               timestamps, component_ids,
                                               log_ids, journal_logs));

    for (size_t j = 0; j < journal_logs.size(); ++j) {
      pending_journal_logs.push_back(journal_logs[j]);
      pending_journal_log_uuids.push_back(log_ids[j]);
      pending_journal_component_uuids.push_back(component_ids[j]);
      pending_journal_timestamps.push_back(timestamps[j]);
    }
  }

  size_t index = 0;
  while (true) {
    auto batch = make_shared<vector<JournalStreamReadLogObject>>();
    auto execution_result =
        mock_journal_input_stream.ReadJournalLogBatch(batch);
    if (!execution_result.Successful()) {
      EXPECT_EQ(
          execution_result,
          FailureExecutionResult(
              errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN));
      break;
    }

    for (size_t i = 0; i < batch->size(); ++i) {
      EXPECT_EQ(batch->at(i).journal_id, 12344321);
      EXPECT_EQ(pending_journal_logs[index].type(),
                batch->at(i).journal_log->type());
      EXPECT_EQ(pending_journal_log_uuids[index], batch->at(i).log_id);
      EXPECT_EQ(pending_journal_component_uuids[index],
                batch->at(i).component_id);
      EXPECT_EQ(pending_journal_timestamps[index], batch->at(i).timestamp);
      index++;
    }
  }
  EXPECT_EQ(index, pending_journal_logs.size());
}

}  // namespace google::scp::core::test
