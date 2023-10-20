/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gmock/gmock-matchers.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>
#include <gtest/gtest_pred_impl.h>

#include <stddef.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/common/auto_expiry_concurrent_map/src/auto_expiry_concurrent_map.h"
#include "core/common/serialization/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/checkpoint_service_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "core/interface/type_def.h"
#include "core/journal_service/mock/mock_journal_service.h"
#include "core/journal_service/src/journal_serialization.h"
#include "core/journal_service/src/proto/journal_service.pb.h"
#include "core/transaction_manager/interface/transaction_phase_manager_interface.h"
#include "core/transaction_manager/mock/mock_transaction_command_serializer.h"
#include "core/transaction_manager/mock/mock_transaction_engine.h"
#include "core/transaction_manager/mock/mock_transaction_manager.h"
#include "core/transaction_manager/src/transaction_engine.h"
#include "pbs/budget_key/mock/mock_budget_key_with_overrides.h"
#include "pbs/budget_key_provider/mock/mock_budget_key_provider.h"
#include "pbs/budget_key_provider/src/budget_key_provider.h"
#include "pbs/checkpoint_service/mock/mock_checkpoint_service.h"
#include "pbs/checkpoint_service/src/error_codes.h"
#include "pbs/interface/budget_key_interface.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

namespace google::scp::pbs::test {
using ::google::scp::core::AsyncContext;
using ::google::scp::core::AsyncExecutorInterface;
using ::google::scp::core::BlobStorageClientInterface;
using ::google::scp::core::BlobStorageProviderInterface;
using ::google::scp::core::Byte;
using ::google::scp::core::BytesBuffer;
using ::google::scp::core::CheckpointId;
using ::google::scp::core::CheckpointLog;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::JournalId;
using ::google::scp::core::JournalLogStatus;
using ::google::scp::core::JournalRecoverRequest;
using ::google::scp::core::JournalRecoverResponse;
using ::google::scp::core::JournalServiceInterface;
using ::google::scp::core::NoSQLDatabaseProviderInterface;
using ::google::scp::core::PutBlobRequest;
using ::google::scp::core::PutBlobResponse;
using ::google::scp::core::RemoteTransactionManagerInterface;
using ::google::scp::core::RetryExecutionResult;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::Timestamp;
using ::google::scp::core::Transaction;
using ::google::scp::core::TransactionCommandSerializerInterface;
using ::google::scp::core::TransactionManagerInterface;
using ::google::scp::core::TransactionRequest;
using ::google::scp::core::async_executor::mock::MockAsyncExecutor;
using ::google::scp::core::blob_storage_provider::mock::MockBlobStorageClient;
using ::google::scp::core::blob_storage_provider::mock::MockBlobStorageProvider;
using ::google::scp::core::common::Uuid;
using ::google::scp::core::config_provider::mock::MockConfigProvider;
using ::google::scp::core::journal_service::CheckpointMetadata;
using ::google::scp::core::journal_service::JournalLog;
using ::google::scp::core::journal_service::JournalSerialization;
using ::google::scp::core::journal_service::LastCheckpointMetadata;
using ::google::scp::core::journal_service::mock::MockJournalService;
using ::google::scp::core::test::IsSuccessfulAndHolds;
using ::google::scp::core::test::ResultIs;
using ::google::scp::core::transaction_manager::TransactionPhase;
using ::google::scp::core::transaction_manager::mock::
    MockTransactionCommandSerializer;
using ::google::scp::core::transaction_manager::mock::MockTransactionEngine;
using ::google::scp::core::transaction_manager::mock::MockTransactionManager;
using ::google::scp::cpio::MockMetricClient;
using ::google::scp::pbs::budget_key::mock::MockBudgetKey;
using ::google::scp::pbs::budget_key_provider::mock::MockBudgetKeyProvider;
using ::google::scp::pbs::checkpoint_service::mock::MockCheckpointService;
using ::std::make_pair;
using ::std::make_shared;
using ::std::shared_ptr;
using ::std::static_pointer_cast;
using ::std::string;
using ::std::vector;

static constexpr char kBucketName[] = "bucket_name";
static constexpr char kPartitionName[] = "partition_name";
static constexpr size_t kInitialBufferSize = 123;

class CheckpointServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    auto bucket_name = make_shared<string>(kBucketName);
    auto partition_name = make_shared<string>(kPartitionName);
    mock_metric_client_ = make_shared<MockMetricClient>();
    mock_config_provider_ = make_shared<MockConfigProvider>();
    auto mock_application_journal_service = make_shared<MockJournalService>();
    auto mock_blob_storage_provider = make_shared<MockBlobStorageProvider>();
    mock_checkpoint_service_ = std::make_unique<MockCheckpointService>(
        bucket_name, partition_name, mock_metric_client_, mock_config_provider_,
        mock_application_journal_service, mock_blob_storage_provider,
        kInitialBufferSize);
  }

  shared_ptr<MockMetricClient> mock_metric_client_;
  shared_ptr<MockConfigProvider> mock_config_provider_;
  std::unique_ptr<MockCheckpointService> mock_checkpoint_service_;
};

TEST_F(CheckpointServiceTest, BootstrapFailure) {
  mock_checkpoint_service_->bootstrap_mock = []() {
    return FailureExecutionResult(123);
  };

  EXPECT_EQ(mock_checkpoint_service_->RunCheckpointWorker(),
            FailureExecutionResult(123));

  mock_checkpoint_service_->bootstrap_mock = []() {
    return RetryExecutionResult(123);
  };

  EXPECT_EQ(mock_checkpoint_service_->RunCheckpointWorker(),
            RetryExecutionResult(123));
}

TEST_F(CheckpointServiceTest, RecoverFailure) {
  mock_checkpoint_service_->bootstrap_mock = []() {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->recover_mock = [](JournalId& journal_id) {
    return FailureExecutionResult(123);
  };

  EXPECT_EQ(mock_checkpoint_service_->RunCheckpointWorker(),
            FailureExecutionResult(123));

  mock_checkpoint_service_->recover_mock = [](JournalId& journal_id) {
    return RetryExecutionResult(123);
  };

  EXPECT_EQ(mock_checkpoint_service_->RunCheckpointWorker(),
            RetryExecutionResult(123));
}

TEST_F(CheckpointServiceTest, CheckpointFailure) {
  mock_checkpoint_service_->bootstrap_mock = []() {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->recover_mock = [](JournalId& journal_id) {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->checkpoint_mock =
      [](JournalId last_processed_journal_id, CheckpointId& checkpoint_id,
         BytesBuffer& last_checkpoint_buffer, BytesBuffer& checkpoint_buffer) {
        return FailureExecutionResult(123);
      };

  EXPECT_EQ(mock_checkpoint_service_->RunCheckpointWorker(),
            FailureExecutionResult(123));

  mock_checkpoint_service_->checkpoint_mock =
      [](JournalId last_processed_journal_id, CheckpointId& checkpoint_id,
         BytesBuffer& last_checkpoint_buffer,
         BytesBuffer& checkpoint_buffer) { return RetryExecutionResult(123); };

  EXPECT_EQ(mock_checkpoint_service_->RunCheckpointWorker(),
            RetryExecutionResult(123));
}

TEST_F(CheckpointServiceTest, StoreFailure) {
  mock_checkpoint_service_->bootstrap_mock = []() {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->recover_mock = [](JournalId& journal_id) {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->checkpoint_mock =
      [](JournalId last_processed_journal_id, CheckpointId& checkpoint_id,
         BytesBuffer& last_checkpoint_buffer,
         BytesBuffer& checkpoint_buffer) { return SuccessExecutionResult(); };

  mock_checkpoint_service_->store_mock = [](CheckpointId& checkpoint_id,
                                            BytesBuffer& last_checkpoint_buffer,
                                            BytesBuffer& checkpoint_buffer) {
    return FailureExecutionResult(123);
  };

  EXPECT_EQ(mock_checkpoint_service_->RunCheckpointWorker(),
            FailureExecutionResult(123));

  mock_checkpoint_service_->store_mock =
      [](CheckpointId& checkpoint_id, BytesBuffer& last_checkpoint_buffer,
         BytesBuffer& checkpoint_buffer) { return RetryExecutionResult(123); };

  EXPECT_EQ(mock_checkpoint_service_->RunCheckpointWorker(),
            RetryExecutionResult(123));
}

TEST_F(CheckpointServiceTest, GetLastPersistedCheckpointIdFailure) {
  mock_checkpoint_service_->bootstrap_mock = []() {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->recover_mock = [](JournalId& journal_id) {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->checkpoint_mock =
      [](JournalId last_processed_journal_id, CheckpointId& checkpoint_id,
         BytesBuffer& last_checkpoint_buffer, BytesBuffer& checkpoint_buffer) {
        checkpoint_id = 123;
        return SuccessExecutionResult();
      };

  mock_checkpoint_service_->store_mock = [](CheckpointId& checkpoint_id,
                                            BytesBuffer& last_checkpoint_buffer,
                                            BytesBuffer& checkpoint_buffer) {
    return FailureExecutionResult(456);
  };

  mock_checkpoint_service_->shutdown_mock = []() {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->RunCheckpointWorker();

  EXPECT_THAT(
      mock_checkpoint_service_->GetLastPersistedCheckpointId(),
      ResultIs(FailureExecutionResult(
          core::errors::
              SC_PBS_CHECKPOINT_SERVICE_INVALID_LAST_PERSISTED_CHECKPOINT_ID)));
}

TEST_F(CheckpointServiceTest, GetLastPersistedCheckpointIdStaleIfStoreFails) {
  mock_checkpoint_service_->bootstrap_mock = []() {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->recover_mock = [](JournalId& journal_id) {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->shutdown_mock = []() {
    return SuccessExecutionResult();
  };

  vector<CheckpointId> checkpoint_ids = {123, 456};
  vector<ExecutionResult> store_mock_results = {SuccessExecutionResult(),
                                                FailureExecutionResult(100)};

  for (int index = 0; index < 2; index++) {
    mock_checkpoint_service_->checkpoint_mock =
        [&](JournalId last_processed_journal_id, CheckpointId& checkpoint_id,
            BytesBuffer& last_checkpoint_buffer,
            BytesBuffer& checkpoint_buffer) {
          checkpoint_id = checkpoint_ids.at(index);
          return SuccessExecutionResult();
        };

    mock_checkpoint_service_->store_mock =
        [&](CheckpointId& checkpoint_id, BytesBuffer& last_checkpoint_buffer,
            BytesBuffer& checkpoint_buffer) {
          return store_mock_results.at(index);
        };

    mock_checkpoint_service_->RunCheckpointWorker();
  }

  EXPECT_THAT(mock_checkpoint_service_->GetLastPersistedCheckpointId(),
              IsSuccessfulAndHolds(checkpoint_ids.at(0)));
}

TEST_F(CheckpointServiceTest, ShutdownFailure) {
  mock_checkpoint_service_->bootstrap_mock = []() {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->recover_mock = [](JournalId& journal_id) {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->checkpoint_mock =
      [](JournalId last_processed_journal_id, CheckpointId& checkpoint_id,
         BytesBuffer& last_checkpoint_buffer,
         BytesBuffer& checkpoint_buffer) { return SuccessExecutionResult(); };

  mock_checkpoint_service_->store_mock =
      [](CheckpointId& checkpoint_id, BytesBuffer& last_checkpoint_buffer,
         BytesBuffer& checkpoint_buffer) { return SuccessExecutionResult(); };

  mock_checkpoint_service_->shutdown_mock = []() {
    return FailureExecutionResult(123);
  };

  EXPECT_EQ(mock_checkpoint_service_->RunCheckpointWorker(),
            FailureExecutionResult(123));

  mock_checkpoint_service_->SetJournalId(123);

  mock_checkpoint_service_->shutdown_mock = []() {
    return RetryExecutionResult(123);
  };

  EXPECT_EQ(mock_checkpoint_service_->RunCheckpointWorker(),
            RetryExecutionResult(123));
}

TEST_F(CheckpointServiceTest, Recover) {
  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  for (const auto& result : results) {
    auto mock_journal_service = make_shared<MockJournalService>();
    mock_journal_service->recover_mock =
        [&](AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
                recover_context) {
          if (result.Successful()) {
            recover_context.response = make_shared<JournalRecoverResponse>();
            recover_context.response->last_processed_journal_id = 12345;
            recover_context.result = SuccessExecutionResult();
            recover_context.Finish();
          }
          return result;
        };

    auto journal_service =
        static_pointer_cast<JournalServiceInterface>(mock_journal_service);
    mock_checkpoint_service_->SetJournalService(journal_service);

    JournalId last_processed_journal_id;
    EXPECT_EQ(mock_checkpoint_service_->Recover(last_processed_journal_id),
              result);

    if (result.Successful()) {
      EXPECT_EQ(last_processed_journal_id, 12345);
    }
  }
}

TEST_F(CheckpointServiceTest, Checkpoint) {
  auto mock_async_executor = make_shared<MockAsyncExecutor>();
  auto async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);
  shared_ptr<JournalServiceInterface> mock_journal_service =
      make_shared<MockJournalService>();
  shared_ptr<TransactionCommandSerializerInterface>
      mock_transaction_command_serializer =
          make_shared<MockTransactionCommandSerializer>();
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager;
  auto mock_transaction_engine = make_shared<MockTransactionEngine>(
      async_executor, mock_transaction_command_serializer, mock_journal_service,
      remote_transaction_manager, mock_metric_client_);

  auto mock_transaction_manager = make_shared<MockTransactionManager>(
      mock_async_executor, mock_transaction_engine, 1000, mock_metric_client_);

  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider = nullptr;
  auto mock_budget_key_provider = make_shared<MockBudgetKeyProvider>(
      async_executor, mock_journal_service, nosql_database_provider,
      mock_metric_client_, mock_config_provider_);
  auto budget_key_provider =
      static_pointer_cast<BudgetKeyProviderInterface>(mock_budget_key_provider);
  auto transaction_manager = static_pointer_cast<TransactionManagerInterface>(
      mock_transaction_manager);

  JournalId last_processed_journal_id = 1234;
  CheckpointId checkpoint_id;
  BytesBuffer last_checkpoint_buffer(1);
  BytesBuffer checkpoint_buffer(1);
  mock_checkpoint_service_->SetBudgetKeyProvider(budget_key_provider);
  mock_checkpoint_service_->SetTransactionManager(transaction_manager);

  EXPECT_EQ(mock_checkpoint_service_->Checkpoint(
                last_processed_journal_id, checkpoint_id,
                last_checkpoint_buffer, checkpoint_buffer),
            FailureExecutionResult(
                core::errors::SC_PBS_CHECKPOINT_SERVICE_NO_LOGS_TO_PROCESS));

  auto transaction_id = Uuid::GenerateUuid();
  auto transaction = make_shared<Transaction>();
  transaction->current_phase = TransactionPhase::Commit;
  transaction->is_coordinated_remotely = true;
  transaction->is_waiting_for_remote = true;
  transaction->context.request = make_shared<TransactionRequest>();
  transaction->context.request->timeout_time = 123456;

  auto pair = make_pair(transaction_id, transaction);
  mock_transaction_engine->GetActiveTransactionsMap().Insert(pair, transaction);

  auto budget_key_name = make_shared<BudgetKeyName>("Budget_Key_Name");
  shared_ptr<BudgetKeyProviderPair> budget_key_provider_pair =
      make_shared<BudgetKeyProviderPair>();
  auto budget_key_id = Uuid::GenerateUuid();
  auto mock_budget_key = make_shared<MockBudgetKey>(
      budget_key_name, budget_key_id, async_executor, mock_journal_service,
      nosql_database_provider, mock_metric_client_, mock_config_provider_);

  budget_key_provider_pair->budget_key =
      static_pointer_cast<BudgetKeyInterface>(mock_budget_key);

  budget_key_provider_pair->is_loaded = false;
  auto budget_key_pair = make_pair(*budget_key_name, budget_key_provider_pair);
  mock_budget_key_provider->GetBudgetKeys()->Insert(budget_key_pair,
                                                    budget_key_provider_pair);

  EXPECT_EQ(mock_checkpoint_service_->Checkpoint(
                last_processed_journal_id, checkpoint_id,
                last_checkpoint_buffer, checkpoint_buffer),
            FailureExecutionResult(
                core::errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE));

  last_checkpoint_buffer.bytes = make_shared<vector<Byte>>(1024 * 1024);
  last_checkpoint_buffer.capacity = 1024 * 1024;

  EXPECT_EQ(mock_checkpoint_service_->Checkpoint(
                last_processed_journal_id, checkpoint_id,
                last_checkpoint_buffer, checkpoint_buffer),
            SuccessExecutionResult());

  EXPECT_NE(checkpoint_id, 0);

  LastCheckpointMetadata last_checkpoint_metadata;
  size_t buffer_offset = 0;
  size_t bytes_deserialized = 0;
  EXPECT_EQ(JournalSerialization::DeserializeLastCheckpointMetadata(
                last_checkpoint_buffer, buffer_offset, last_checkpoint_metadata,
                bytes_deserialized),
            SuccessExecutionResult());
  EXPECT_EQ(last_checkpoint_metadata.last_checkpoint_id(), checkpoint_id);

  CheckpointMetadata checkpoint_metadata;
  buffer_offset = 0;
  bytes_deserialized = 0;
  EXPECT_EQ(JournalSerialization::DeserializeCheckpointMetadata(
                checkpoint_buffer, buffer_offset, checkpoint_metadata,
                bytes_deserialized),
            SuccessExecutionResult());
  EXPECT_EQ(checkpoint_metadata.last_processed_journal_id(),
            last_processed_journal_id);

  // To remove the checkpoint
  checkpoint_buffer.length -= bytes_deserialized;

  buffer_offset = 0;
  size_t total_logs = 0;
  while (buffer_offset < checkpoint_buffer.length) {
    Timestamp timestamp;
    JournalLogStatus log_status;
    Uuid component_id;
    Uuid log_id;
    size_t bytes_deserialized = 0;
    EXPECT_EQ(JournalSerialization::DeserializeLogHeader(
                  checkpoint_buffer, buffer_offset, timestamp, log_status,
                  component_id, log_id, bytes_deserialized),
              SuccessExecutionResult());

    // Log body should be skipped.
    buffer_offset += bytes_deserialized;

    JournalLog journal_log;
    bytes_deserialized = 0;
    EXPECT_EQ(
        JournalSerialization::DeserializeJournalLog(
            checkpoint_buffer, buffer_offset, journal_log, bytes_deserialized),
        SuccessExecutionResult());
    buffer_offset += bytes_deserialized;

    total_logs++;
  }

  EXPECT_EQ(total_logs, 4);
}

TEST_F(CheckpointServiceTest, WriteBlob) {
  auto mock_blob_storage_client = make_shared<MockBlobStorageClient>();
  auto blob_storage_client =
      static_pointer_cast<BlobStorageClientInterface>(mock_blob_storage_client);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  for (const auto& result : results) {
    auto blob_name = make_shared<string>("blob_name");
    auto bytes_buffer = make_shared<BytesBuffer>(1);

    mock_blob_storage_client->put_blob_mock =
        [&](AsyncContext<PutBlobRequest, PutBlobResponse>& context) {
          EXPECT_EQ(*context.request->blob_name, *blob_name);
          EXPECT_EQ(*context.request->bucket_name, kBucketName);
          EXPECT_EQ(context.request->buffer->capacity, 1);
          EXPECT_EQ(context.request->buffer->bytes->size(), 1);

          if (result.Successful()) {
            context.result = SuccessExecutionResult();
            context.Finish();
          }
          return result;
        };

    EXPECT_EQ(mock_checkpoint_service_->WriteBlob(blob_storage_client,
                                                  blob_name, bytes_buffer),
              result);
  }
}

TEST_F(CheckpointServiceTest, StoreBlob) {
  shared_ptr<BlobStorageProviderInterface> blob_storage_provider =
      make_shared<MockBlobStorageProvider>();
  mock_checkpoint_service_->SetBlobStorageProvider(blob_storage_provider);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  for (const auto& result : results) {
    auto blob_name = make_shared<string>("blob_name");
    auto bytes_buffer = make_shared<BytesBuffer>(1);
    BytesBuffer last_checkpoint_buffer(1);
    BytesBuffer checkpoint_buffer(2);

    size_t call_count = 0;
    mock_checkpoint_service_->write_blob_mock =
        [&](shared_ptr<BlobStorageClientInterface>& blob_storage_client,
            shared_ptr<string>& blob_name,
            shared_ptr<BytesBuffer>& bytes_buffer) {
          if (result.Successful()) {
            if (call_count++ == 0) {
              EXPECT_EQ(*blob_name,
                        "partition_name/checkpoint_00000000000000123456");
              EXPECT_EQ(bytes_buffer->capacity, 2);
            } else {
              EXPECT_EQ(*blob_name, "partition_name/last_checkpoint");
              EXPECT_EQ(bytes_buffer->capacity, 1);
            }
          }
          return result;
        };

    CheckpointId checkpoint_id = 123456;
    EXPECT_EQ(mock_checkpoint_service_->Store(
                  checkpoint_id, last_checkpoint_buffer, checkpoint_buffer),
              result);
  }
}

TEST_F(CheckpointServiceTest, GetLastPersistedCheckpointId) {
  mock_checkpoint_service_->bootstrap_mock = []() {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->recover_mock = [](JournalId& journal_id) {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->checkpoint_mock =
      [](JournalId last_processed_journal_id, CheckpointId& checkpoint_id,
         BytesBuffer& last_checkpoint_buffer, BytesBuffer& checkpoint_buffer) {
        checkpoint_id = 123;
        return SuccessExecutionResult();
      };

  mock_checkpoint_service_->store_mock =
      [](CheckpointId& checkpoint_id, BytesBuffer& last_checkpoint_buffer,
         BytesBuffer& checkpoint_buffer) { return SuccessExecutionResult(); };

  mock_checkpoint_service_->shutdown_mock = []() {
    return SuccessExecutionResult();
  };

  mock_checkpoint_service_->RunCheckpointWorker();

  EXPECT_THAT(mock_checkpoint_service_->GetLastPersistedCheckpointId(),
              IsSuccessfulAndHolds(123));
}

TEST_F(CheckpointServiceTest, Shutdown) {
  EXPECT_SUCCESS(mock_checkpoint_service_->Shutdown());
}
}  // namespace google::scp::pbs::test
