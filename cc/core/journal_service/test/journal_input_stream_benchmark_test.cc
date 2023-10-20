// Copyright 2023 Google LLC
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

#include <filesystem>

#include <benchmark/benchmark.h>

#include "absl/synchronization/notification.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/config_provider/src/env_config_provider.h"
#include "core/journal_service/src/journal_input_stream.h"
#include "core/journal_service/test/test_util.h"
#include "public/core/test/interface/execution_result_matchers.h"

namespace google::scp::core::test {

using ::google::scp::core::blob_storage_provider::mock::MockBlobStorageClient;
using ::google::scp::core::journal_service::JournalLog;
using ::google::scp::core::journal_service::JournalStreamReadLogRequest;
using ::google::scp::core::journal_service::JournalStreamReadLogResponse;

constexpr char kBucketName[] = "fake_bucket";
constexpr char kPartitionName[] = "fake_partition";

class JournalInputStreamBaseFixture : public benchmark::Fixture {
 protected:
  void SetUp(const benchmark::State& state) override {
    std::filesystem::path partition_dir_path = kBucketName;
    std::filesystem::create_directory(partition_dir_path);
    mock_storage_client_ = std::make_shared<MockBlobStorageClient>();
  }

  void TearDown(const benchmark::State& state) override {
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

  void WriteCheckpointAndJournals(uint64_t number_of_journals) {
    EXPECT_SUCCESS(journal_service::test_util::WriteLastCheckpoint(
        /*checkpoint_id=*/1, *mock_storage_client_));

    JournalLog journal_log_1;
    journal_log_1.set_type(1);
    EXPECT_SUCCESS(journal_service::test_util::WriteCheckpoint(
        journal_log_1,
        /*last_processed_journal_id=*/1,
        journal_service::test_util::JournalIdToString(1),
        *mock_storage_client_));

    for (int i = 1000; i < 1000 + number_of_journals; i++) {
      std::vector<JournalLog> journals(10);
      for (int j = 0; j < 10; j++) {
        JournalLog journal_log;
        journal_log.set_type(j);
        journals[j] = journal_log;
      }
      EXPECT_SUCCESS(journal_service::test_util::WriteJournalLogs(
          journals, journal_service::test_util::JournalIdToString(i),
          *mock_storage_client_));
    }
  }

  ExecutionResultOr<size_t> ReadLogs(JournalInputStream& journal_input_stream) {
    JournalStreamReadLogRequest request;
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        context;
    context.request = std::make_shared<JournalStreamReadLogRequest>(request);
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>
        callback_context;

    absl::Notification notification;
    context.callback = [&](AsyncContext<JournalStreamReadLogRequest,
                                        JournalStreamReadLogResponse>&
                               journal_stream_read_log_context) {
      callback_context = journal_stream_read_log_context;
      notification.Notify();
    };
    if (auto execution_result = journal_input_stream.ReadLog(context);
        !execution_result.Successful()) {
      return execution_result;
    }
    notification.WaitForNotification();
    if (callback_context.result.Successful()) {
      return callback_context.response->read_logs->size();
    }
    return callback_context.result;
  }

  std::shared_ptr<MockBlobStorageClient> mock_storage_client_;
};

class JournalInputStreamWithStreamingReadFixture
    : public JournalInputStreamBaseFixture {
 protected:
  void SetUp(const benchmark::State& state) override {
    JournalInputStreamBaseFixture::SetUp(state);

    setenv(kPBSJournalInputStreamNumberOfJournalLogsToReturn, "1000",
           /*replace=*/1);
    setenv(kPBSJournalInputStreamNumberOfJournalsPerBatch, "1000",
           /*replace=*/1);
    setenv(kPBSJournalInputStreamEnableBatchReadJournals, "true",
           /*replace=*/1);
    journal_input_stream_ = CreateJournalInputStream();
    WriteCheckpointAndJournals(state.range(0));
  }

  std::unique_ptr<JournalInputStream> journal_input_stream_;
};

static constexpr char kReadLogCountKey[] = "read_logs_count";
static constexpr char kJournalLogsReadCountKey[] = "journal_logs_read_count";
static constexpr int kBenchmarkStart = 1 << 10;
static constexpr int kBenchmarkLimit = 1 << 13;
static constexpr int kBenchmarkStep = 1 << 10;

BENCHMARK_DEFINE_F(JournalInputStreamWithStreamingReadFixture, Read)

(benchmark::State& state) {
  uint64_t read_logs_count = 1;
  uint64_t jounal_logs_count = 0;
  for (auto _ : state) {
    ExecutionResultOr<size_t> number_of_journal_logs =
        ReadLogs(*journal_input_stream_);
    while (number_of_journal_logs.Successful()) {
      jounal_logs_count += *number_of_journal_logs;
      number_of_journal_logs = ReadLogs(*journal_input_stream_);
      read_logs_count++;
    }
  }
  state.counters[kReadLogCountKey] = read_logs_count;
  state.counters[kJournalLogsReadCountKey] = jounal_logs_count;
}

BENCHMARK_REGISTER_F(JournalInputStreamWithStreamingReadFixture, Read)
    ->DenseRange(kBenchmarkStart, kBenchmarkLimit, kBenchmarkStep);

class JournalInputStreamWithoutStreamingReadFixture
    : public JournalInputStreamBaseFixture {
 protected:
  void SetUp(const benchmark::State& state) override {
    JournalInputStreamBaseFixture::SetUp(state);

    setenv(kPBSJournalInputStreamNumberOfJournalLogsToReturn, "1000",
           /*replace=*/1);
    setenv(kPBSJournalInputStreamNumberOfJournalsPerBatch, "1000",
           /*replace=*/1);
    setenv(kPBSJournalInputStreamEnableBatchReadJournals, "false",
           /*replace=*/1);
    journal_input_stream_ = CreateJournalInputStream();
    WriteCheckpointAndJournals(state.range(0));
  }

  std::unique_ptr<JournalInputStream> journal_input_stream_;
};

BENCHMARK_DEFINE_F(JournalInputStreamWithoutStreamingReadFixture, Read)

(benchmark::State& state) {
  uint64_t read_logs_count = 0;
  uint64_t jounal_logs_count = 0;
  for (auto _ : state) {
    ExecutionResultOr<size_t> number_of_journal_logs =
        ReadLogs(*journal_input_stream_);
    while (number_of_journal_logs.Successful()) {
      jounal_logs_count += *number_of_journal_logs;
      number_of_journal_logs = ReadLogs(*journal_input_stream_);
      read_logs_count++;
    }
  }
  state.counters[kReadLogCountKey] = read_logs_count;
  state.counters[kJournalLogsReadCountKey] = jounal_logs_count;
}

BENCHMARK_REGISTER_F(JournalInputStreamWithoutStreamingReadFixture, Read)
    ->DenseRange(kBenchmarkStart, kBenchmarkLimit, kBenchmarkStep);

}  // namespace google::scp::core::test

// Run the benchmark
BENCHMARK_MAIN();
