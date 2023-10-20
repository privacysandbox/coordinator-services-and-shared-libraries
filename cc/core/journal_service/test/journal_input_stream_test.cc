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

#include <stdlib.h>

#include <atomic>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "cc/core/config_provider/src/env_config_provider.h"
#include "cc/core/test/utils/proto_test_utils.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/common/serialization/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/type_def.h"
#include "core/journal_service/interface/journal_service_stream_interface.h"
#include "core/journal_service/mock/mock_journal_input_stream.h"
#include "core/journal_service/src/error_codes.h"
#include "core/journal_service/src/journal_serialization.h"
#include "core/journal_service/src/journal_utils.h"
#include "core/journal_service/src/proto/journal_service.pb.h"
#include "core/journal_service/test/test_util.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::JournalLogStatus;
using google::scp::core::Timestamp;
using google::scp::core::blob_storage_provider::mock::MockBlobStorageClient;
using google::scp::core::common::Uuid;
using google::scp::core::journal_service::CheckpointMetadata;
using google::scp::core::journal_service::JournalLog;
using google::scp::core::journal_service::JournalSerialization;
using google::scp::core::journal_service::JournalStreamReadLogObject;
using google::scp::core::journal_service::JournalStreamReadLogRequest;
using google::scp::core::journal_service::JournalStreamReadLogResponse;
using google::scp::core::journal_service::LastCheckpointMetadata;
using google::scp::core::journal_service::mock::MockJournalInputStream;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::ResultIs;
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

constexpr char kBucketName[] = "fake_bucket";
constexpr char kPartitionName[] = "fake_partition";

struct TestCase {
  std::string test_name;
  bool enable_batch_read;
  int seed;
};

class JournalInputStreamTest : public testing::Test {
 protected:
  void SetUp() override {
    std::filesystem::path partition_dir_path = kBucketName;
    std::filesystem::create_directory(partition_dir_path);
    mock_storage_client_ = std::make_shared<MockBlobStorageClient>();
    journal_input_stream_ = CreateJournalInputStream();
  }

  void TearDown() override {
    std::filesystem::path partition_dir_path = kBucketName;
    std::filesystem::remove_all(partition_dir_path);
  }

  std::unique_ptr<JournalInputStream> CreateJournalInputStream() {
    return std::make_unique<JournalInputStream>(
        std::make_shared<std::string>(kBucketName),
        std::make_shared<std::string>(kPartitionName),
        std::shared_ptr<BlobStorageClientInterface>(mock_storage_client_),
        std::make_shared<EnvConfigProvider>());
  }

  ExecutionResult WriteFile(const BytesBuffer& bytes_buffer,
                            std::string_view file_name) {
    return journal_service::test_util::WriteFile(bytes_buffer, file_name,
                                                 *mock_storage_client_);
  }

  static ExecutionResultOr<BytesBuffer> JournalLogToBytesBuffer(
      const JournalLog& journal_log) {
    return journal_service::test_util::JournalLogToBytesBuffer(journal_log);
  }

  ExecutionResult WriteJournalLog(const JournalLog& journal_log,
                                  std::string_view journal_file_postfix) {
    return journal_service::test_util::WriteJournalLogs(
        {journal_log}, journal_file_postfix, *mock_storage_client_);
  }

  ExecutionResult WriteJournalLogs(const std::vector<JournalLog>& journal_logs,
                                   std::string_view journal_file_postfix) {
    return journal_service::test_util::WriteJournalLogs(
        journal_logs, journal_file_postfix, *mock_storage_client_);
  }

  ExecutionResult WriteCheckpoint(const JournalLog& journal_log,
                                  JournalId last_processed_journal_id,
                                  std::string_view file_postfix) {
    return journal_service::test_util::WriteCheckpoint(
        journal_log, last_processed_journal_id, file_postfix,
        *mock_storage_client_);
  }

  ExecutionResult WriteLastCheckpoint(CheckpointId checkpoint_id) {
    return journal_service::test_util::WriteLastCheckpoint(
        checkpoint_id, *mock_storage_client_);
  }

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
  ReadLogs() {
    return ReadLogs(JournalStreamReadLogRequest());
  }

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
  ReadLogsWithFlagProcessCheckpointWhenNoJournalsIsDisabled() {
    JournalStreamReadLogRequest request;
    request.should_read_stream_when_only_checkpoint_exists = false;
    return ReadLogs(request);
  }

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
  ReadLogs(const JournalStreamReadLogRequest& request) {
    return journal_service::test_util::ReadLogs(request,
                                                *journal_input_stream_);
  }

  ExecutionResult ReadLogsExecutionResult() {
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        context;
    context.request = std::make_shared<JournalStreamReadLogRequest>();

    context.callback = [&](AsyncContext<JournalStreamReadLogRequest,
                                        JournalStreamReadLogResponse>&
                               journal_stream_read_log_context) {};
    return journal_input_stream_->ReadLog(context);
  }

  static std::string IdToString(uint64_t journal_id) {
    return journal_service::test_util::JournalIdToString(journal_id);
  }

  void ExpectNoMoreLogsToReturn() {
    for (int i = 0; i < 3; ++i) {
      auto execution_result = ReadLogsExecutionResult();
      EXPECT_THAT(
          execution_result,
          FailureExecutionResult(
              core::errors::
                  SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN));
    }
  }

  std::shared_ptr<MockBlobStorageClient> mock_storage_client_;
  std::unique_ptr<JournalInputStream> journal_input_stream_;
};

class JournalInputStreamTestWithParam
    : public JournalInputStreamTest,
      public testing::WithParamInterface<TestCase> {
  void SetUp() override {
    setenv(kPBSJournalInputStreamNumberOfJournalLogsToReturn, "5000",
           /*replace=*/1);
    setenv(kPBSJournalInputStreamNumberOfJournalsPerBatch, "1000",
           /*replace=*/1);
    setenv(kPBSJournalInputStreamEnableBatchReadJournals,
           GetParam().enable_batch_read ? "true" : "false",
           /*replace=*/1);

    JournalInputStreamTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(
    JournalInputStreamTestWithParam, JournalInputStreamTestWithParam,
    testing::ValuesIn<TestCase>({
        {"EnableBatchReadJournals", true},
        {"DisableBatchReadJournals", false},
    }),
    [](const testing::TestParamInfo<JournalInputStreamTestWithParam::ParamType>&
           info) { return info.param.test_name; });

TEST_P(JournalInputStreamTestWithParam,
       ReadLogsWithoutAnyCheckpointAndJournalsSuccess) {
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs();
  EXPECT_THAT(context.result,
              FailureExecutionResult(
                  core::errors::
                      SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN));
  EXPECT_EQ(context.retry_count, 0);

  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam, ReadLogsWithOneJournals) {
  JournalLog journal_log;
  journal_log.set_type(11);
  EXPECT_SUCCESS(WriteJournalLog(journal_log, IdToString(1)));

  {
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        context = ReadLogs();

    EXPECT_SUCCESS(context.result);
    ASSERT_TRUE(context.response != nullptr);
    ASSERT_TRUE(context.response->read_logs != nullptr);
    ASSERT_EQ(context.response->read_logs->size(), 1);
    EXPECT_EQ(context.retry_count, 0);

    EXPECT_THAT(*context.response->read_logs->at(0).journal_log,
                EqualsProto(journal_log));
    EXPECT_EQ(context.response->read_logs->at(0).journal_id, 1);
  }
  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam, ReadLogsWithTwoJournals) {
  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  EXPECT_SUCCESS(WriteJournalLog(journal_log_1, IdToString(1)));

  JournalLog journal_log_2;
  journal_log_2.set_type(22);
  EXPECT_SUCCESS(WriteJournalLog(journal_log_2, IdToString(2)));

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs();

  EXPECT_SUCCESS(context.result);
  ASSERT_TRUE(context.response != nullptr);
  ASSERT_TRUE(context.response->read_logs != nullptr);
  ASSERT_EQ(context.response->read_logs->size(), 2);
  EXPECT_EQ(context.retry_count, 0);

  EXPECT_THAT(*context.response->read_logs->at(0).journal_log,
              EqualsProto(journal_log_1));
  EXPECT_EQ(context.response->read_logs->at(0).journal_id, 1);

  EXPECT_THAT(*context.response->read_logs->at(1).journal_log,
              EqualsProto(journal_log_2));
  EXPECT_EQ(context.response->read_logs->at(1).journal_id, 2);

  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam,
       ReadLogsWithTwoJournalBlobsAndFourJournalLogs) {
  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  JournalLog journal_log_2;
  journal_log_2.set_type(22);
  EXPECT_SUCCESS(
      WriteJournalLogs({journal_log_1, journal_log_2}, IdToString(1)));

  JournalLog journal_log_3;
  journal_log_3.set_type(33);
  JournalLog journal_log_4;
  journal_log_4.set_type(44);
  EXPECT_SUCCESS(
      WriteJournalLogs({journal_log_3, journal_log_4}, IdToString(3)));

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs();

  EXPECT_SUCCESS(context.result);
  ASSERT_TRUE(context.response != nullptr);
  ASSERT_TRUE(context.response->read_logs != nullptr);
  ASSERT_EQ(context.response->read_logs->size(), 4);
  EXPECT_EQ(context.retry_count, 0);

  EXPECT_THAT(*context.response->read_logs->at(0).journal_log,
              EqualsProto(journal_log_1));
  EXPECT_EQ(context.response->read_logs->at(0).journal_id, 1);

  EXPECT_THAT(*context.response->read_logs->at(1).journal_log,
              EqualsProto(journal_log_2));
  EXPECT_EQ(context.response->read_logs->at(1).journal_id, 1);

  EXPECT_THAT(*context.response->read_logs->at(2).journal_log,
              EqualsProto(journal_log_3));
  EXPECT_EQ(context.response->read_logs->at(2).journal_id, 3);

  EXPECT_THAT(*context.response->read_logs->at(3).journal_log,
              EqualsProto(journal_log_4));
  EXPECT_EQ(context.response->read_logs->at(3).journal_id, 3);

  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam, ReadLogsWithTwoJournalLogsToReturn) {
  setenv(kPBSJournalInputStreamNumberOfJournalsPerBatch, "4", /*replace=*/1);
  setenv(kPBSJournalInputStreamNumberOfJournalLogsToReturn, "2",
         /*replace=*/1);
  journal_input_stream_ = CreateJournalInputStream();

  for (int i = 1; i <= 15; i += 2) {
    JournalLog journal_log_1;
    journal_log_1.set_type(i);
    JournalLog journal_log_2;
    journal_log_2.set_type(i + 1);
    EXPECT_SUCCESS(
        WriteJournalLogs({journal_log_1, journal_log_2}, IdToString(i)));
  }

  for (int i = 1; i <= 15; i += 2) {
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        context = ReadLogs();
    EXPECT_SUCCESS(context.result);
    ASSERT_TRUE(context.response != nullptr);
    ASSERT_TRUE(context.response->read_logs != nullptr);
    ASSERT_EQ(context.response->read_logs->size(), 2);

    JournalLog journal_log_1;
    journal_log_1.set_type(i);
    JournalLog journal_log_2;
    journal_log_2.set_type(i + 1);

    EXPECT_THAT(*context.response->read_logs->at(0).journal_log,
                EqualsProto(journal_log_1));
    EXPECT_EQ(context.response->read_logs->at(0).journal_id, i);
    EXPECT_THAT(*context.response->read_logs->at(1).journal_log,
                EqualsProto(journal_log_2));
    EXPECT_EQ(context.response->read_logs->at(1).journal_id, i);
  }

  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam,
       ReadLogsWithUnevenNumberOfJournalLogsPerBlob) {
  setenv(kPBSJournalInputStreamNumberOfJournalLogsToReturn, "2",
         /*replace=*/1);
  journal_input_stream_ = CreateJournalInputStream();

  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  JournalLog journal_log_2;
  journal_log_2.set_type(22);
  JournalLog journal_log_3;
  journal_log_3.set_type(33);
  EXPECT_SUCCESS(WriteJournalLogs({journal_log_1, journal_log_2, journal_log_3},
                                  IdToString(1)));

  JournalLog journal_log_4;
  journal_log_4.set_type(44);
  JournalLog journal_log_5;
  journal_log_5.set_type(55);
  EXPECT_SUCCESS(
      WriteJournalLogs({journal_log_4, journal_log_5}, IdToString(3)));

  {
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        context = ReadLogs();
    EXPECT_SUCCESS(context.result);
    ASSERT_TRUE(context.response != nullptr);
    ASSERT_TRUE(context.response->read_logs != nullptr);
    ASSERT_EQ(context.response->read_logs->size(), 2);

    EXPECT_THAT(*context.response->read_logs->at(0).journal_log,
                EqualsProto(journal_log_1));
    EXPECT_EQ(context.response->read_logs->at(0).journal_id, 1);

    EXPECT_THAT(*context.response->read_logs->at(1).journal_log,
                EqualsProto(journal_log_2));
    EXPECT_EQ(context.response->read_logs->at(1).journal_id, 1);
  }
  {
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        context = ReadLogs();
    EXPECT_SUCCESS(context.result);
    ASSERT_TRUE(context.response != nullptr);
    ASSERT_TRUE(context.response->read_logs != nullptr);
    ASSERT_EQ(context.response->read_logs->size(), 2);

    EXPECT_THAT(*context.response->read_logs->at(0).journal_log,
                EqualsProto(journal_log_3));
    EXPECT_EQ(context.response->read_logs->at(0).journal_id, 1);

    EXPECT_THAT(*context.response->read_logs->at(1).journal_log,
                EqualsProto(journal_log_4));
    EXPECT_EQ(context.response->read_logs->at(1).journal_id, 3);
  }
  {
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        context = ReadLogs();
    EXPECT_SUCCESS(context.result);
    ASSERT_TRUE(context.response != nullptr);
    ASSERT_TRUE(context.response->read_logs != nullptr);
    ASSERT_EQ(context.response->read_logs->size(), 1);
    EXPECT_THAT(*context.response->read_logs->at(0).journal_log,
                EqualsProto(journal_log_5));
    EXPECT_EQ(context.response->read_logs->at(0).journal_id, 3);
  }

  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam,
       ReadLogsWithJournalIdGreaterThanMaxJournalId) {
  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  EXPECT_SUCCESS(WriteJournalLog(journal_log_1, IdToString(1)));

  JournalLog journal_log_2;
  journal_log_2.set_type(22);
  EXPECT_SUCCESS(WriteJournalLog(journal_log_2, IdToString(2)));

  JournalStreamReadLogRequest request;
  request.max_journal_id_to_process = 1;
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs(request);

  EXPECT_SUCCESS(context.result);
  ASSERT_TRUE(context.response != nullptr);
  ASSERT_TRUE(context.response->read_logs != nullptr);
  ASSERT_EQ(context.response->read_logs->size(), 1);
  EXPECT_EQ(context.retry_count, 0);

  EXPECT_THAT(*context.response->read_logs->at(0).journal_log,
              EqualsProto(journal_log_1));
  EXPECT_EQ(context.response->read_logs->at(0).journal_id, 1);

  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam,
       ReadLogsOneCheckpointWhenFlagProcessCheckpointNoJournalsIsDisabled) {
  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  EXPECT_SUCCESS(WriteCheckpoint(journal_log_1, /*last_processed_journal_id=*/1,
                                 IdToString(1)));

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogsWithFlagProcessCheckpointWhenNoJournalsIsDisabled();

  // When there is one checkpoint file, there is no journal logs after the
  // checkpoint file, and the checkpoint file has JournalLog and
  // CheckpointMetadata, JournalInputStream will read the CheckpointMetadata
  // and returns an End of Stream.
  EXPECT_THAT(context.result,
              FailureExecutionResult(
                  core::errors::
                      SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN));
  EXPECT_EQ(context.retry_count, 0);

  for (int i = 0; i < 3; ++i) {
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        context;
    context.request = std::make_shared<JournalStreamReadLogRequest>();
    context.request->should_read_stream_when_only_checkpoint_exists = false;

    context.callback = [&](AsyncContext<JournalStreamReadLogRequest,
                                        JournalStreamReadLogResponse>&
                               journal_stream_read_log_context) {};
    auto execution_result = journal_input_stream_->ReadLog(context);
    if (GetParam().enable_batch_read) {
      EXPECT_THAT(
          execution_result,
          FailureExecutionResult(
              core::errors::
                  SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN));
    } else {
      if (i == 0) {
        EXPECT_SUCCESS(execution_result);
      } else {
        EXPECT_THAT(
            execution_result,
            FailureExecutionResult(
                core::errors::
                    SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN));
      }
    }
  }
}

TEST_P(JournalInputStreamTestWithParam, ReadLogsWithOneCheckpoint) {
  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  EXPECT_SUCCESS(WriteCheckpoint(journal_log_1, /*last_processed_journal_id=*/1,
                                 IdToString(1)));

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs();

  // When there is one checkpoint file, there is no journal logs after the
  // checkpoint file, and the checkpoint file has JournalLog and
  // CheckpointMetadata, JournalInputStream will read the CheckpointMetadata
  // and also reads the JournalLog in the checkpoint file.
  ASSERT_TRUE(context.response != nullptr);
  ASSERT_TRUE(context.response->read_logs != nullptr);
  EXPECT_SUCCESS(context.result);
  ASSERT_EQ(context.response->read_logs->size(), 1);
  EXPECT_EQ(context.retry_count, 0);

  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam,
       ReadLogsWithOneCheckpointAndOneJournal) {
  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  EXPECT_SUCCESS(WriteCheckpoint(journal_log_1, /*last_processed_journal_id=*/1,
                                 IdToString(1)));

  JournalLog journal_log_2;
  journal_log_2.set_type(22);
  EXPECT_SUCCESS(WriteJournalLog(journal_log_2, IdToString(2)));
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs();

  EXPECT_SUCCESS(context.result);
  ASSERT_TRUE(context.response != nullptr);
  ASSERT_TRUE(context.response->read_logs != nullptr);
  ASSERT_EQ(context.response->read_logs->size(), 2);
  EXPECT_EQ(context.retry_count, 0);

  EXPECT_THAT(*context.response->read_logs->at(0).journal_log,
              EqualsProto(journal_log_1));
  EXPECT_EQ(context.response->read_logs->at(0).journal_id, 1);

  EXPECT_THAT(*context.response->read_logs->at(1).journal_log,
              EqualsProto(journal_log_2));
  EXPECT_EQ(context.response->read_logs->at(1).journal_id, 2);

  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam,
       ReadLogsWithOneCheckpointAndOneJournalBeforeCheckpoint) {
  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  EXPECT_SUCCESS(WriteCheckpoint(
      journal_log_1, /*last_processed_journal_id=*/11, IdToString(11)));

  JournalLog journal_log_2;
  journal_log_2.set_type(22);
  EXPECT_SUCCESS(WriteJournalLog(journal_log_2, IdToString(2)));
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs();

  // When there is one checkpoint file, there is no journal logs after the
  // checkpoint file, and the checkpoint file has JournalLog and
  // CheckpointMetadata, JournalInputStream will read the CheckpointMetadata
  // and also reads the JournalLog in the checkpoint file.
  EXPECT_SUCCESS(context.result);
  ASSERT_TRUE(context.response != nullptr);
  ASSERT_TRUE(context.response->read_logs != nullptr);
  ASSERT_EQ(context.response->read_logs->size(), 1);
  EXPECT_EQ(context.retry_count, 0);

  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam, ReadLogsWithTwoCheckpoint) {
  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  EXPECT_SUCCESS(WriteCheckpoint(journal_log_1, /*last_processed_journal_id=*/1,
                                 IdToString(1)));

  JournalLog journal_log_2;
  journal_log_2.set_type(22);
  EXPECT_SUCCESS(WriteCheckpoint(journal_log_2, /*last_processed_journal_id=*/2,
                                 IdToString(2)));

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs();

  // When there is one checkpoint file, there is no journal logs after the
  // checkpoint file, and the checkpoint file has JournalLog and
  // CheckpointMetadata, JournalInputStream will read the CheckpointMetadata
  // and also reads the JournalLog in the checkpoint file.
  ASSERT_TRUE(context.response != nullptr);
  ASSERT_TRUE(context.response->read_logs != nullptr);
  ASSERT_EQ(context.response->read_logs->size(), 1);
  ASSERT_EQ(context.response->read_logs->at(0).journal_id, 2);
  EXPECT_SUCCESS(context.result);
  EXPECT_EQ(context.retry_count, 0);

  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam,
       ReadLogsWithOneLastCheckpointTwoCheckpointAndOneJournal) {
  EXPECT_SUCCESS(WriteLastCheckpoint(/*checkpoint_id=*/2));

  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  EXPECT_SUCCESS(WriteCheckpoint(journal_log_1, /*last_processed_journal_id=*/1,
                                 IdToString(1)));

  JournalLog journal_log_2;
  journal_log_2.set_type(22);
  EXPECT_SUCCESS(WriteCheckpoint(journal_log_2, /*last_processed_journal_id=*/2,
                                 IdToString(2)));

  JournalLog journal_log_3;
  journal_log_3.set_type(33);
  EXPECT_SUCCESS(WriteJournalLog(journal_log_3, IdToString(3)));

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs();

  EXPECT_SUCCESS(context.result);
  ASSERT_TRUE(context.response != nullptr);
  ASSERT_TRUE(context.response->read_logs != nullptr);
  ASSERT_EQ(context.response->read_logs->size(), 2);
  EXPECT_EQ(context.retry_count, 0);

  EXPECT_THAT(*context.response->read_logs->at(0).journal_log,
              EqualsProto(journal_log_2));
  EXPECT_EQ(context.response->read_logs->at(0).journal_id, 2);

  EXPECT_THAT(*context.response->read_logs->at(1).journal_log,
              EqualsProto(journal_log_3));
  EXPECT_EQ(context.response->read_logs->at(1).journal_id, 3);

  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam,
       ReadLogsWithLastCheckpointPointingAtNonExistCheckpoint) {
  EXPECT_SUCCESS(WriteLastCheckpoint(/*checkpoint_id=*/5));

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs();

  EXPECT_THAT(context.result,
              ResultIs(FailureExecutionResult(
                  errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND)));
}

TEST_P(JournalInputStreamTestWithParam, ReadLogsWithManyJournalLogs) {
  EXPECT_SUCCESS(WriteLastCheckpoint(/*checkpoint_id=*/1));

  JournalLog journal_log_1;
  journal_log_1.set_type(1);
  EXPECT_SUCCESS(WriteCheckpoint(journal_log_1, /*last_processed_journal_id=*/1,
                                 IdToString(1)));

  for (int i = 1000; i < 3000; i++) {
    JournalLog journal_log;
    journal_log.set_type(i);
    EXPECT_SUCCESS(WriteJournalLog(journal_log, IdToString(i)));
  }

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs();
  EXPECT_SUCCESS(context.result);
  ASSERT_TRUE(context.response != nullptr);
  ASSERT_TRUE(context.response->read_logs != nullptr);
  if (GetParam().enable_batch_read) {
    // 1000 journal logs + 1 checkpoint log
    ASSERT_EQ(context.response->read_logs->size(), 1001);

    EXPECT_EQ(context.response->read_logs->at(0).journal_id, 1);
    // Inspecting journal_ids from [1, 1001]
    for (int i = 0; i < 1000; i++) {
      ASSERT_EQ(context.response->read_logs->at(i + 1).journal_id, 1000 + i);
    }

    context = ReadLogs();
    EXPECT_SUCCESS(context.result);
    ASSERT_TRUE(context.response != nullptr);
    ASSERT_TRUE(context.response->read_logs != nullptr);
    ASSERT_EQ(context.response->read_logs->size(), 1000);

    // Inspecting journal_ids from [1002, 1999]
    for (int i = 0; i < 1000; i++) {
      ASSERT_EQ(context.response->read_logs->at(i).journal_id, 2000 + i);
    }
  } else {
    ASSERT_EQ(context.response->read_logs->size(), 2001);
  }
  ExpectNoMoreLogsToReturn();
}

class JournalInputStreamTestWithRandomSeed
    : public JournalInputStreamTest,
      public testing::WithParamInterface<TestCase> {
 protected:
  void SetUp() override {
    seed_ = GetParam().seed;
    setenv(kPBSJournalInputStreamNumberOfJournalLogsToReturn,
           std::to_string(1 + rand_r(&seed_) % 5000).c_str(),
           /*replace=*/1);
    setenv(kPBSJournalInputStreamNumberOfJournalsPerBatch,
           std::to_string(1 + rand_r(&seed_) % 5000).c_str(),
           /*replace=*/1);
    setenv(kPBSJournalInputStreamEnableBatchReadJournals,
           GetParam().enable_batch_read ? "true" : "false",
           /*replace=*/1);

    JournalInputStreamTest::SetUp();
  }

  uint32_t seed_;
};

INSTANTIATE_TEST_SUITE_P(
    JournalInputStreamTestWithRandomSeed, JournalInputStreamTestWithRandomSeed,
    testing::ValuesIn<TestCase>({
        {"EnableBatchReadJournalsRandomSeed1", true, 1},
        {"EnableBatchReadJournalsRandomSeed2", true, 2},
        {"EnableBatchReadJournalsRandomSeed3", true, 3},
        {"EnableBatchReadJournalsRandomSeed4", true, 4},
        {"EnableBatchReadJournalsRandomSeed5", true, 5},
        {"DisableBatchReadJournalsRandomSeed1", false, 1},
        {"DisableBatchReadJournalsRandomSeed2", false, 2},
        {"DisableBatchReadJournalsRandomSeed3", false, 3},
        {"DisableBatchReadJournalsRandomSeed4", false, 4},
        {"DisableBatchReadJournalsRandomSeed5", false, 5},
    }),
    [](const testing::TestParamInfo<
        JournalInputStreamTestWithRandomSeed::ParamType>& info) {
      return info.param.test_name;
    });

TEST_P(JournalInputStreamTestWithRandomSeed,
       ReadLogsWithPseudoRandomNumberOfJournals) {
  EXPECT_SUCCESS(WriteLastCheckpoint(/*checkpoint_id=*/1));

  JournalLog journal_log_1;
  journal_log_1.set_type(1);
  EXPECT_SUCCESS(WriteCheckpoint(journal_log_1, /*last_processed_journal_id=*/1,
                                 IdToString(1)));

  std::vector<JournalLog> all_journals;
  all_journals.push_back(journal_log_1);
  for (int i = 1000; i < 1001 + rand_r(&seed_) % 1000; i++) {
    std::vector<JournalLog> journals;
    for (int j = 0; j < 1 + rand_r(&seed_) % 10; j++) {
      JournalLog journal_log;
      journal_log.set_type(j);
      journals.push_back(journal_log);
      all_journals.push_back(journal_log);
    }
    EXPECT_SUCCESS(WriteJournalLogs(journals, IdToString(i)));
  }

  size_t journal_index = 0;
  while (journal_index < all_journals.size()) {
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        callback_context;
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        context;
    context.request = std::make_shared<JournalStreamReadLogRequest>();

    absl::Notification notification;
    context.callback = [&](AsyncContext<JournalStreamReadLogRequest,
                                        JournalStreamReadLogResponse>&
                               journal_stream_read_log_context) {
      callback_context = journal_stream_read_log_context;
      notification.Notify();
    };
    if (auto execution_result = journal_input_stream_->ReadLog(context);
        !execution_result.Successful()) {
      break;
    }
    notification.WaitForNotificationWithTimeout(absl::Seconds(3));

    if (!callback_context.result.Successful()) {
      break;
    }

    ASSERT_TRUE(callback_context.response != nullptr);
    ASSERT_TRUE(callback_context.response->read_logs != nullptr);
    for (const JournalStreamReadLogObject& log :
         *callback_context.response->read_logs) {
      EXPECT_TRUE(log.journal_log != nullptr);
      EXPECT_THAT(*log.journal_log, EqualsProto(all_journals[journal_index++]));
    }
  }

  EXPECT_GE(journal_index, all_journals.size());

  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam, ReadLogsCorruptedJournalLog) {
  EXPECT_SUCCESS(WriteLastCheckpoint(/*checkpoint_id=*/1));

  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  EXPECT_SUCCESS(WriteCheckpoint(journal_log_1, /*last_processed_journal_id=*/1,
                                 IdToString(1)));

  JournalLog journal_log;
  journal_log.set_type(2);
  EXPECT_SUCCESS(WriteJournalLog(journal_log, IdToString(2)));

  BytesBuffer corrupted_journal_log("corrupted");
  WriteFile(corrupted_journal_log,
            absl::StrCat(kJournalBlobNamePrefix, IdToString(3)));

  JournalLog journal_log_2;
  journal_log_2.set_type(4);
  EXPECT_SUCCESS(WriteJournalLog(journal_log_2, IdToString(4)));

  {
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        callback_context;
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        context;
    context.request = std::make_shared<JournalStreamReadLogRequest>();

    absl::Notification notification;
    context.callback = [&](AsyncContext<JournalStreamReadLogRequest,
                                        JournalStreamReadLogResponse>&
                               journal_stream_read_log_context) {
      callback_context = journal_stream_read_log_context;
      notification.Notify();
    };
    EXPECT_SUCCESS(journal_input_stream_->ReadLog(context));
    notification.WaitForNotification();

    EXPECT_THAT(callback_context.result,
                ResultIs(FailureExecutionResult(
                    errors::SC_SERIALIZATION_BUFFER_NOT_READABLE)));
    EXPECT_TRUE(callback_context.response == nullptr);
  }

  {
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        context;
    context.request = std::make_shared<JournalStreamReadLogRequest>();
    EXPECT_THAT(journal_input_stream_->ReadLog(context),
                ResultIs(FailureExecutionResult(
                    errors::SC_SERIALIZATION_BUFFER_NOT_READABLE)));
  }
}

TEST_P(JournalInputStreamTestWithParam, ReadLogsNoMoreLogs) {
  EXPECT_SUCCESS(WriteLastCheckpoint(/*checkpoint_id=*/1));

  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  EXPECT_SUCCESS(WriteCheckpoint(journal_log_1, /*last_processed_journal_id=*/1,
                                 IdToString(1)));

  JournalLog journal_log;
  journal_log.set_type(2);
  EXPECT_SUCCESS(WriteJournalLog(journal_log, IdToString(2)));

  {
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        callback_context;
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        context;
    context.request = std::make_shared<JournalStreamReadLogRequest>();

    absl::Notification notification;
    context.callback = [&](AsyncContext<JournalStreamReadLogRequest,
                                        JournalStreamReadLogResponse>&
                               journal_stream_read_log_context) {
      callback_context = journal_stream_read_log_context;
      notification.Notify();
    };
    EXPECT_SUCCESS(journal_input_stream_->ReadLog(context));
    notification.WaitForNotification();

    EXPECT_SUCCESS(callback_context.result);
    ASSERT_TRUE(callback_context.response != nullptr);
    ASSERT_TRUE(callback_context.response->read_logs != nullptr);
    ASSERT_EQ(callback_context.response->read_logs->size(), 2);
  }
  ExpectNoMoreLogsToReturn();
}

TEST_P(JournalInputStreamTestWithParam, GetLastCheckpointBlobFailed) {
  auto expected_result = FailureExecutionResult(123);
  mock_storage_client_->get_blob_mock =
      [&](AsyncContext<GetBlobRequest, GetBlobResponse>&) {
        return expected_result;
      };
  EXPECT_THAT(ReadLogsExecutionResult(), ResultIs(expected_result));
}

TEST_P(JournalInputStreamTestWithParam, GetLastCheckpointBlobCallbackFailed) {
  auto expected_result = FailureExecutionResult(123);
  mock_storage_client_->get_blob_mock =
      [&](AsyncContext<GetBlobRequest, GetBlobResponse>& context) {
        context.result = expected_result;
        context.callback(context);
        return SuccessExecutionResult();
      };
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs();
  EXPECT_THAT(context.result, ResultIs(expected_result));
}

TEST_P(JournalInputStreamTestWithParam, ReadLogsMaxNumberOfJournalsToProcess) {
  EXPECT_SUCCESS(WriteLastCheckpoint(/*checkpoint_id=*/1));

  JournalLog journal_log_1;
  journal_log_1.set_type(11);
  EXPECT_SUCCESS(WriteCheckpoint(journal_log_1, /*last_processed_journal_id=*/1,
                                 IdToString(1)));

  for (int i = 2; i < 1000; i++) {
    JournalLog journal_log;
    journal_log.set_type(i);
    EXPECT_SUCCESS(WriteJournalLog(journal_log, IdToString(i)));
  }

  JournalStreamReadLogRequest request;
  request.max_number_of_journals_to_process = 500;
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      context = ReadLogs(request);

  EXPECT_SUCCESS(context.result);
  ASSERT_TRUE(context.response != nullptr);
  ASSERT_TRUE(context.response->read_logs != nullptr);

  // 500 journals + 1 journal in the checkpoint file = 501
  ASSERT_EQ(context.response->read_logs->size(), 501);

  ExpectNoMoreLogsToReturn();
}

class MockJournalInputStreamTest : public testing::Test {
 protected:
  void SetUp() override {
    auto bucket_name = make_shared<string>("bucket_name");
    auto partition_name = make_shared<string>("partition_name");
    mock_storage_client_ = make_shared<MockBlobStorageClient>();
    shared_ptr<BlobStorageClientInterface> storage_client_ =
        mock_storage_client_;
    mock_journal_input_stream_ = make_shared<MockJournalInputStream>(
        bucket_name, partition_name, storage_client_,
        std::make_shared<EnvConfigProvider>());
  }

  shared_ptr<MockBlobStorageClient> mock_storage_client_;
  shared_ptr<MockJournalInputStream> mock_journal_input_stream_;
};

class MockJournalInputStreamTestWithParam
    : public MockJournalInputStreamTest,
      public testing::WithParamInterface<TestCase> {
  void SetUp() override {
    setenv(kPBSJournalInputStreamEnableBatchReadJournals,
           GetParam().enable_batch_read ? "true" : "false",
           /*replace=*/1);
    MockJournalInputStreamTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(
    MockJournalInputStreamTestWithParam, MockJournalInputStreamTestWithParam,
    testing::ValuesIn<TestCase>({
        {"EnableBatchReadJournals", true},
        {"DisableBatchReadJournals", false},
    }),
    [](const testing::TestParamInfo<
        MockJournalInputStreamTestWithParam::ParamType>& info) {
      return info.param.test_name;
    });

TEST_P(MockJournalInputStreamTestWithParam, ReadLastCheckpointBlob) {
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
        bucket_name, partition_name, storage_client,
        std::make_shared<EnvConfigProvider>());

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    EXPECT_EQ(mock_journal_input_stream.ReadLastCheckpointBlob(
                  journal_stream_read_log_context),
              result);
  }
}

TEST_P(MockJournalInputStreamTestWithParam,
       OnReadLastCheckpointBlobCallbackBlobNotFound) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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
          EXPECT_THAT(journal_stream_read_log_context.result, ResultIs(result));
          condition = true;
        };
    mock_journal_input_stream.OnReadLastCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(MockJournalInputStreamTestWithParam, OnReadLastCheckpointListFails) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
  // When the result is blob not found, but list result is not
  // successful
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
          EXPECT_THAT(journal_stream_read_log_context.result, ResultIs(result));
          condition = true;
        };
    mock_journal_input_stream.OnReadLastCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(MockJournalInputStreamTestWithParam, OnReadLastCheckpointBlobCorrupted) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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

TEST_P(MockJournalInputStreamTestWithParam,
       OnReadLastCheckpointBlobReadBlobFails) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
  // When the result is blob found and the content looks good but
  // reading the checkpoint file immediately fails
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
          EXPECT_THAT(journal_stream_read_log_context.result, ResultIs(result));
          condition = true;
        };
    mock_journal_input_stream.OnReadLastCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(MockJournalInputStreamTestWithParam, ReadCheckpointBlob) {
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
        bucket_name, partition_name, storage_client,
        std::make_shared<EnvConfigProvider>());

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    EXPECT_EQ(mock_journal_input_stream.ReadCheckpointBlob(
                  journal_stream_read_log_context, 100 /* checkpoint_id */),
              result);
  }
}

TEST_P(MockJournalInputStreamTestWithParam, OnReadCheckpointBlobCallback) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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
          EXPECT_THAT(journal_stream_read_log_context.result, ResultIs(result));
          condition = true;
        };
    mock_journal_input_stream.OnReadCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(MockJournalInputStreamTestWithParam, OnReadCheckpointBlobCorruptedBlob) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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
            EXPECT_THAT(journal_stream_read_log_context.result,
                        ResultIs(FailureExecutionResult(
                            errors::SC_SERIALIZATION_BUFFER_NOT_READABLE)));
          } else {
            EXPECT_THAT(
                journal_stream_read_log_context.result,
                ResultIs(FailureExecutionResult(
                    errors::SC_JOURNAL_SERVICE_MAGIC_NUMBER_NOT_MATCHING)));
          }
          condition = true;
        };
    mock_journal_input_stream.OnReadCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(MockJournalInputStreamTestWithParam, OnReadCheckpointBlobListBlobsFail) {
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
        bucket_name, partition_name, storage_client,
        std::make_shared<EnvConfigProvider>());
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
                    "partition_name/"
                    "journal_00000000000000001234");
          return result;
        };

    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_THAT(journal_stream_read_log_context.result, ResultIs(result));
          condition = true;
        };
    mock_journal_input_stream.OnReadCheckpointBlobCallback(
        journal_stream_read_log_context, get_blob_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(MockJournalInputStreamTestWithParam, ListCheckpoints) {
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
        bucket_name, partition_name, storage_client,
        std::make_shared<EnvConfigProvider>());

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

TEST_P(MockJournalInputStreamTestWithParam, OnListCheckpointsCallback) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));

  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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
          EXPECT_THAT(journal_stream_read_log_context.result, ResultIs(result));
          condition = true;
        };
    mock_journal_input_stream.OnListCheckpointsCallback(
        journal_stream_read_log_context, list_blobs_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(MockJournalInputStreamTestWithParam,
       OnListCheckpointsCallbackListFails) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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
          // Because there were no checkpoints in the
          // listing, all the journals are read, i.e.
          // start_from will be nullptr.
          EXPECT_EQ(start_from, nullptr);
          return result;
        };

    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_THAT(journal_stream_read_log_context.result, ResultIs(result));
          condition = true;
        };

    mock_journal_input_stream.OnListCheckpointsCallback(
        journal_stream_read_log_context, list_blobs_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(MockJournalInputStreamTestWithParam,
       OnListCheckpointsCallbackWrongBlobNames) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
  // When the result is success but there are checkpoint blobs with
  // wrong name
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
        EXPECT_THAT(journal_stream_read_log_context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME)));
        condition = true;
      };
  mock_journal_input_stream.OnListCheckpointsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_P(MockJournalInputStreamTestWithParam,
       OnListCheckpointsCallbackInvalidIndex) {
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
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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
        EXPECT_THAT(journal_stream_read_log_context.result,
                    ResultIs(FailureExecutionResult(123)));
        condition = true;
      };

  mock_journal_input_stream.OnListCheckpointsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_P(MockJournalInputStreamTestWithParam,
       OnListCheckpointsCallbackWithMarker) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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
          EXPECT_THAT(journal_stream_read_log_context.result, ResultIs(result));
          condition = true;
        };
    mock_journal_input_stream.OnListCheckpointsCallback(
        journal_stream_read_log_context, list_blobs_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(MockJournalInputStreamTestWithParam, ListJournals) {
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
        bucket_name, partition_name, storage_client,
        std::make_shared<EnvConfigProvider>());

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

TEST_P(MockJournalInputStreamTestWithParam, OnListJournalsCallback) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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
          EXPECT_THAT(journal_stream_read_log_context.result, ResultIs(result));
          condition = true;
        };
    mock_journal_input_stream.OnListJournalsCallback(
        journal_stream_read_log_context, list_blobs_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(MockJournalInputStreamTestWithParam,
       OnListJournalsCallbackNoJournalBlobs) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());

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
        EXPECT_THAT(
            journal_stream_read_log_context.result,
            FailureExecutionResult(
                core::errors::
                    SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN));
        condition = true;
      };

  mock_journal_input_stream.OnListJournalsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_P(MockJournalInputStreamTestWithParam,
       OnListJournalsCallbackWrongBlobNames) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
  // When the result is success but there are checkpoint blobs with
  // wrong name
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
        EXPECT_THAT(journal_stream_read_log_context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME)));
        condition = true;
      };
  mock_journal_input_stream.OnListJournalsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_P(MockJournalInputStreamTestWithParam,
       OnListJournalsCallbackProperListing) {
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
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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
        EXPECT_EQ(journal_ids[1], 12315);
        EXPECT_EQ(journal_ids[2], 12333312315);
        EXPECT_EQ(mock_journal_input_stream.GetLastProcessedJournalId(),
                  12333312315);
        return FailureExecutionResult(123);
      };

  journal_stream_read_log_context.callback =
      [&](AsyncContext<JournalStreamReadLogRequest,
                       JournalStreamReadLogResponse>&
              journal_stream_read_log_context) {
        EXPECT_THAT(journal_stream_read_log_context.result,
                    ResultIs(FailureExecutionResult(123)));
        condition = true;
      };
  mock_journal_input_stream.OnListJournalsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_P(MockJournalInputStreamTestWithParam,
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
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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
        EXPECT_THAT(journal_stream_read_log_context.result,
                    ResultIs(FailureExecutionResult(123)));
        condition = true;
      };
  mock_journal_input_stream.OnListJournalsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_P(MockJournalInputStreamTestWithParam,
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
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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
        EXPECT_THAT(journal_stream_read_log_context.result,
                    ResultIs(FailureExecutionResult(123)));
        condition = true;
      };
  mock_journal_input_stream.OnListJournalsCallback(
      journal_stream_read_log_context, list_blobs_context);
  WaitUntil([&]() { return condition.load(); });
}

TEST_P(MockJournalInputStreamTestWithParam, OnListJournalsCallbackWithMarker) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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
          EXPECT_THAT(journal_stream_read_log_context.result, ResultIs(result));
          condition = true;
        };
    mock_journal_input_stream.OnListJournalsCallback(
        journal_stream_read_log_context, list_blobs_context);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(MockJournalInputStreamTestWithParam,
       ReadJournalBlobsWithEmptyBlobsList) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  journal_stream_read_log_context.callback = [&](auto&) {};

  vector<uint64_t> journal_ids;
  if (GetParam().enable_batch_read) {
    EXPECT_THAT(
        mock_journal_input_stream.ReadJournalBlobs(
            journal_stream_read_log_context, journal_ids),
        ResultIs(FailureExecutionResult(
            errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN)));
  } else {
    EXPECT_THAT(mock_journal_input_stream.ReadJournalBlobs(
                    journal_stream_read_log_context, journal_ids),
                ResultIs(FailureExecutionResult(
                    errors::SC_JOURNAL_SERVICE_INPUT_STREAM_INVALID_LISTING)));
  }
}

TEST_P(MockJournalInputStreamTestWithParam, ReadJournalBlobsProperly) {
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
        bucket_name, partition_name, storage_client,
        std::make_shared<EnvConfigProvider>());
    mock_journal_input_stream.set_journal_ids_loaded(true);
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

    // ReadJournalBlobs will read each journal_ids in order
    // and assumes that the id in the journal_ids vector is
    // already sorted.
    EXPECT_EQ(journal_id_dispatched[0], 100);
    EXPECT_EQ(journal_id_dispatched[1], 10);
    EXPECT_EQ(journal_id_dispatched[2], 24);

    EXPECT_EQ(buffer_index_dispatched[0], 0);
    EXPECT_EQ(buffer_index_dispatched[1], 1);
    EXPECT_EQ(buffer_index_dispatched[2], 2);
  }
}

TEST_P(MockJournalInputStreamTestWithParam, ReadJournalBlobsFailedToSchedule) {
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  journal_stream_read_log_context.callback = [&](auto&) {};
  mock_journal_input_stream_->set_journal_ids_loaded(true);
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
  // This function call should not return a Failure as there are two
  // more callbacks to be recieved for the journals 10 and 24 and the
  // last one would finish the context.
  EXPECT_THAT(mock_journal_input_stream_->ReadJournalBlobs(
                  journal_stream_read_log_context, journal_ids),
              ResultIs(SuccessExecutionResult()));
}

TEST_P(MockJournalInputStreamTestWithParam, ReadJournalBlob) {
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
        bucket_name, partition_name, storage_client,
        std::make_shared<EnvConfigProvider>());

    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    EXPECT_EQ(mock_journal_input_stream.ReadJournalBlob(
                  journal_stream_read_log_context, 100 /* journal_id */,
                  10 /* buffer_index */),
              result);
  }
}

TEST_P(MockJournalInputStreamTestWithParam, OnReadJournalBlobCallback) {
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
        bucket_name, partition_name, storage_client,
        std::make_shared<EnvConfigProvider>());
    atomic<bool> condition(false);
    AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
    get_blob_context.result = result;
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        journal_stream_read_log_context;
    journal_stream_read_log_context.callback =
        [&](AsyncContext<JournalStreamReadLogRequest,
                         JournalStreamReadLogResponse>&
                journal_stream_read_log_context) {
          EXPECT_THAT(journal_stream_read_log_context.result,
                      ResultIs(FailureExecutionResult(1234)));
          condition = true;
        };
    mock_journal_input_stream.GetTotalJournalsToRead() = 1;
    mock_journal_input_stream.OnReadJournalBlobCallback(
        journal_stream_read_log_context, get_blob_context, 1);
    WaitUntil([&]() { return condition.load(); });
  }
}

TEST_P(MockJournalInputStreamTestWithParam,
       OnReadJournalBlobCallbackDifferentBuffers) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());

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

TEST_P(MockJournalInputStreamTestWithParam, ProcessLoadedJournals) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());

  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  mock_journal_input_stream.process_next_journal_log_mock =
      [](Timestamp&, JournalLogStatus&, Uuid&, Uuid&, JournalLog&, JournalId&) {
        return FailureExecutionResult(1234);
      };
  EXPECT_THAT(mock_journal_input_stream.ProcessLoadedJournals(
                  journal_stream_read_log_context),
              ResultIs(FailureExecutionResult(1234)));
}

TEST_P(MockJournalInputStreamTestWithParam, ProcessLoadedJournalsProperly) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());

  atomic<bool> condition(false);
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  journal_stream_read_log_context.callback =
      [&](AsyncContext<JournalStreamReadLogRequest,
                       JournalStreamReadLogResponse>&
              journal_stream_read_log_context) {
        EXPECT_SUCCESS(journal_stream_read_log_context.result);
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

TEST_P(MockJournalInputStreamTestWithParam,
       ProcessLoadedJournalsSerializationFailure) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());

  BytesBuffer buffer;
  buffer.bytes = make_shared<vector<Byte>>();
  buffer.capacity = 0;
  buffer.length = 1;
  mock_journal_input_stream.GetJournalBuffers().push_back(buffer);
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  journal_stream_read_log_context.request =
      std::make_shared<JournalStreamReadLogRequest>();
  EXPECT_EQ(
      mock_journal_input_stream.ProcessLoadedJournals(
          journal_stream_read_log_context),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
}

TEST_P(MockJournalInputStreamTestWithParam,
       ProcessLoadedJournalsSerializationFailure2) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());

  BytesBuffer buffer(12);
  buffer.length = 12;
  mock_journal_input_stream.GetJournalBuffers().push_back(buffer);
  AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
      journal_stream_read_log_context;
  journal_stream_read_log_context.request =
      std::make_shared<JournalStreamReadLogRequest>();
  EXPECT_EQ(
      mock_journal_input_stream.ProcessLoadedJournals(
          journal_stream_read_log_context),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
  EXPECT_EQ(mock_journal_input_stream.GetJournalsLoaded(), false);
}

TEST_P(MockJournalInputStreamTestWithParam, ProcessNextJournalLog) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");
  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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

TEST_P(MockJournalInputStreamTestWithParam, ProcessNextJournalLogProperly) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());
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

TEST_P(MockJournalInputStreamTestWithParam,
       ProcessNextJournalLogSerializeAndDeserialize) {
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());

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
      EXPECT_THAT(
          execution_result,
          ResultIs(FailureExecutionResult(
              errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN)));
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

TEST_F(MockJournalInputStreamTest, ReadJournalLogBatch) {
  // This test case is not applicable when EnableBatchReadJournals is
  // set to true because ReadJournalLogBatch (which is responsible for
  // deserializing journal blob that is already stored in memory and
  // notifying the callback) will not be called repeatedly and directly
  // when the feature flag is set to true.
  //
  // Instead, ReadJournalBlobs (which is responsible for reading journal
  // blob from blob storage, deserializing journal blobs, and notifying
  // the callback) will be called repeatedly when the feature flag is
  // set to true.
  setenv(kPBSJournalInputStreamEnableBatchReadJournals, "false",
         /*replace=*/1);
  auto bucket_name = make_shared<string>("bucket_name");
  auto partition_name = make_shared<string>("partition_name");

  MockBlobStorageClient mock_storage_client;
  shared_ptr<BlobStorageClientInterface> storage_client =
      make_shared<MockBlobStorageClient>(move(mock_storage_client));
  MockJournalInputStream mock_journal_input_stream(
      bucket_name, partition_name, storage_client,
      std::make_shared<EnvConfigProvider>());

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
      EXPECT_THAT(
          execution_result,
          ResultIs(FailureExecutionResult(
              errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN)));
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
