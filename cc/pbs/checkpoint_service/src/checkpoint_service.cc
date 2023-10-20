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

#include "checkpoint_service.h"

// IWYU pragma: no_include <bits/chrono.h>
#include <atomic>
#include <chrono>  // IWYU pragma: keep
#include <future>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/async_executor/src/async_executor.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/serialization/src/error_codes.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/interface/service_interface.h"
#include "core/journal_service/src/error_codes.h"
#include "core/journal_service/src/journal_serialization.h"
#include "core/journal_service/src/journal_service.h"
#include "core/journal_service/src/journal_utils.h"
#include "core/journal_service/src/proto/journal_service.pb.h"
#include "core/transaction_manager/src/transaction_manager.h"
#include "pbs/budget_key_provider/src/budget_key_provider.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/transactions/src/transaction_command_serializer.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::BlobStorageClientInterface;
using google::scp::core::BlobStorageProviderInterface;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::CheckpointId;
using google::scp::core::CheckpointLog;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::JournalId;
using google::scp::core::JournalRecoverRequest;
using google::scp::core::JournalRecoverResponse;
using google::scp::core::JournalService;
using google::scp::core::JournalServiceInterface;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::PutBlobRequest;
using google::scp::core::PutBlobResponse;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::TransactionCommandSerializerInterface;
using google::scp::core::TransactionManager;
using google::scp::core::TransactionManagerInterface;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::Uuid;
using google::scp::core::journal_service::CheckpointMetadata;
using google::scp::core::journal_service::JournalLog;
using google::scp::core::journal_service::JournalSerialization;
using google::scp::core::journal_service::JournalUtils;
using google::scp::core::journal_service::LastCheckpointMetadata;
using std::future;
using std::list;
using std::make_shared;
using std::move;
using std::promise;
using std::shared_ptr;
using std::string;
using std::thread;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

// TODO: Use configuration provider to update the following.
static constexpr char kLastCheckpointBlobName[] = "last_checkpoint";
static constexpr char kCheckpointService[] = "CheckpointService";
static constexpr size_t kBufferIncreaseThreshold = 1 * 1024 * 1024;  // 1MB
static constexpr size_t kDefaultCheckpointIntervalInSeconds = 5;
static constexpr size_t kDefaultMaxJournalsToCheckpointInEachRun = 1000;

namespace google::scp::pbs {
ExecutionResult CheckpointService::Init() noexcept {
  if (!config_provider_
           ->Get(kPBSJournalCheckpointingIntervalInSeconds,
                 checkpointing_interval_in_seconds_)
           .Successful()) {
    checkpointing_interval_in_seconds_ = kDefaultCheckpointIntervalInSeconds;
  }

  if (!config_provider_
           ->Get(kPBSJournalCheckpointingMaxJournalEntriesToProcessInEachRun,
                 max_journals_to_process_in_each_checkpoint_run_)
           .Successful()) {
    max_journals_to_process_in_each_checkpoint_run_ =
        kDefaultMaxJournalsToCheckpointInEachRun;
  }

  if (auto execution_result = FromString(*partition_name_, partition_id_);
      !execution_result.Successful()) {
    SCP_ERROR(kCheckpointService, kZeroUuid, execution_result,
              "Invalid partition name '%s'", partition_name_->c_str());
    return execution_result;
  }

  SCP_INFO(kCheckpointService, partition_id_,
           "Starting Checkpoint Service for Partition with ID: '%s'. "
           "Checkpointing Interval in Seconds: %zu, "
           "Number of journal entries to process in each checkpoint run: %zu",
           ToString(partition_id_).c_str(), checkpointing_interval_in_seconds_,
           max_journals_to_process_in_each_checkpoint_run_);

  return SuccessExecutionResult();
};

ExecutionResult CheckpointService::Run() noexcept {
  if (is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_CHECKPOINT_SERVICE_IS_ALREADY_RUNNING);
  }

  is_running_ = true;
  thread checkpoint_thread([&]() {
    while (is_running_) {
      // This operation must be done synchronously.
      activity_id_ = Uuid::GenerateUuid();

      SCP_INFO(kCheckpointService, activity_id_,
               "Starting checkpointing activity for Partition with ID: '%s'",
               ToString(partition_id_).c_str());

      auto execution_result = RunCheckpointWorker();
      Shutdown();

      if (!execution_result.Successful()) {
        if ((execution_result.status_code !=
             core::errors::SC_JOURNAL_SERVICE_NO_NEW_JOURNAL_ID_AVAILABLE) &&
            (execution_result.status_code !=
             core::errors::SC_PBS_CHECKPOINT_SERVICE_NO_LOGS_TO_PROCESS)) {
          // TODO: Create an alert.
          SCP_ERROR(kCheckpointService, activity_id_, execution_result,
                    "Checkpointing failed.");
          continue;
        }
      }

      if (!is_running_) {
        break;
      }

      // If it was successful sleep for the interval.
      std::this_thread::sleep_for(
          std::chrono::seconds(checkpointing_interval_in_seconds_));
    }
  });
  worker_thread_ = move(checkpoint_thread);
  return SuccessExecutionResult();
};

ExecutionResult CheckpointService::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(
        core::errors::SC_PBS_CHECKPOINT_SERVICE_IS_ALREADY_STOPPED);
  }

  is_running_ = false;
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  return SuccessExecutionResult();
};

ExecutionResult CheckpointService::RunCheckpointWorker() noexcept {
  auto checkpoint_round_start_timestamp =
      TimeProvider::GetSteadyTimestampInNanoseconds();
  auto execution_result = Bootstrap();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  JournalId last_processed_journal_id;
  execution_result = Recover(last_processed_journal_id);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  SCP_INFO(
      kCheckpointService, activity_id_,
      "Checkpoint run's Journal Recovery finished. "
      "Last processed journal id: %llu. Time taken to recover: '%llu' (ms)",
      last_processed_journal_id_,
      duration_cast<milliseconds>(
          TimeProvider::GetSteadyTimestampInNanoseconds() -
          checkpoint_round_start_timestamp)
          .count());

  if (last_processed_journal_id == last_processed_journal_id_) {
    SCP_INFO(kCheckpointService, activity_id_,
             "Last processed journal in this recovery run is same as the one "
             "authored in the most recent checkpointing activity. "
             "Nothing new to checkpoint.");
    return SuccessExecutionResult();
  }

  auto checkpoint_generation_start_timestamp =
      TimeProvider::GetSteadyTimestampInNanoseconds();

  CheckpointId checkpoint_id;
  BytesBuffer checkpoint_buffer(initial_buffer_size_);
  BytesBuffer last_check_point_buffer(initial_buffer_size_);
  execution_result = Checkpoint(last_processed_journal_id, checkpoint_id,
                                last_check_point_buffer, checkpoint_buffer);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  SCP_INFO(kCheckpointService, activity_id_,
           "Checkpoint buffer constructed. Size (bytes): '%llu', Time taken to "
           "construct: "
           "'%llu' (ms)",
           checkpoint_buffer.Size(),
           duration_cast<milliseconds>(
               TimeProvider::GetSteadyTimestampInNanoseconds() -
               checkpoint_generation_start_timestamp)
               .count());

  execution_result =
      Store(checkpoint_id, last_check_point_buffer, checkpoint_buffer);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  last_processed_journal_id_ = last_processed_journal_id;
  last_persisted_checkpoint_id_ = checkpoint_id;
  SCP_INFO(kCheckpointService, activity_id_,
           "Partition with ID: '%s' Checkpointing Done. "
           "Last processed journal id: '%llu'. Last persisted checkpoint id: "
           "'%llu'. "
           "Time taken for this checkpoint run: '%llu' (ms)",
           ToString(partition_id_).c_str(), last_processed_journal_id_,
           last_persisted_checkpoint_id_,
           duration_cast<milliseconds>(
               TimeProvider::GetSteadyTimestampInNanoseconds() -
               checkpoint_round_start_timestamp)
               .count());
  return Shutdown();
}

void CheckpointService::CreateComponents() noexcept {
  async_executor_ = make_shared<AsyncExecutor>(4, 100000);
  io_async_executor_ = make_shared<AsyncExecutor>(8, 100000);
  journal_service_ = make_shared<JournalService>(
      bucket_name_, partition_name_, async_executor_, blob_storage_provider_,
      metric_client_, config_provider_);
  budget_key_provider_ = make_shared<BudgetKeyProvider>(
      async_executor_, journal_service_, metric_client_, config_provider_);
  transaction_command_serializer_ = make_shared<TransactionCommandSerializer>(
      async_executor_, budget_key_provider_);
  transaction_manager_ = make_shared<TransactionManager>(
      async_executor_, transaction_command_serializer_, journal_service_,
      remote_transaction_manager_, 100000, metric_client_, config_provider_,
      partition_id_);
}

ExecutionResult CheckpointService::Bootstrap() noexcept {
  CreateComponents();

  auto execution_result = async_executor_->Init();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  execution_result = io_async_executor_->Init();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  execution_result = journal_service_->Init();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  execution_result = budget_key_provider_->Init();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  execution_result = transaction_manager_->Init();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  execution_result = async_executor_->Run();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  execution_result = io_async_executor_->Run();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult CheckpointService::Recover(
    JournalId& last_processed_journal_id) noexcept {
  JournalId max_journal_id_to_process;

  // If there is no activity on application after restart, we cannot get the
  // last persisted journal ID, so we defer the checkpointing for later when the
  // traffic starts.
  auto execution_result =
      application_journal_service_->GetLastPersistedJournalId(
          max_journal_id_to_process);
  if (!execution_result.Successful()) {
    SCP_INFO(kCheckpointService, activity_id_,
             "LastPersistedJournalId not available. Not checkpointing.");
    return execution_result;
  }

  promise<ExecutionResult> recovery_execution_result;
  AsyncContext<JournalRecoverRequest, JournalRecoverResponse> recovery_context;
  recovery_context.request = make_shared<JournalRecoverRequest>();
  recovery_context.request->max_journal_id_to_process =
      max_journal_id_to_process;
  recovery_context.request->max_number_of_journals_to_process =
      max_journals_to_process_in_each_checkpoint_run_;
  // If there is only a checkpoint to recover, we do not need to Recover because
  // there is nothing to be checkpointed after recovery.
  recovery_context.request
      ->should_perform_recovery_with_only_checkpoint_in_stream = false;
  recovery_context.parent_activity_id = activity_id_;
  recovery_context.correlation_id = activity_id_;
  recovery_context.callback =
      [&](AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
              recovery_context) {
        if (recovery_context.result.Successful()) {
          last_processed_journal_id =
              recovery_context.response->last_processed_journal_id;
        }
        recovery_execution_result.set_value(recovery_context.result);
      };

  // Recovering the service
  // Recovery metrics needs to be separately Run because the journal_service_ is
  // not yet Run().
  RETURN_IF_FAILURE(journal_service_->RunRecoveryMetrics());
  RETURN_IF_FAILURE(journal_service_->Recover(recovery_context));
  auto future_result = recovery_execution_result.get_future().get();
  RETURN_IF_FAILURE(journal_service_->StopRecoveryMetrics());

  return future_result;
}

ExecutionResult CheckpointService::Checkpoint(
    JournalId last_processed_journal_id, CheckpointId& checkpoint_id,
    BytesBuffer& last_checkpoint_buffer,
    BytesBuffer& checkpoint_buffer) noexcept {
  LastCheckpointMetadata last_checkpoint_metadata;
  CheckpointMetadata checkpoint_metadata;
  auto checkpoint_logs = make_shared<list<CheckpointLog>>();
  auto execution_result = transaction_manager_->Checkpoint(checkpoint_logs);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  execution_result = budget_key_provider_->Checkpoint(checkpoint_logs);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  if (checkpoint_logs->size() == 0) {
    SCP_INFO(
        kCheckpointService, activity_id_,
        "No new checkpoint logs found from transaction manager "
        "and budget key provider. No new checkpoint file will be created.");
    return FailureExecutionResult(
        core::errors::SC_PBS_CHECKPOINT_SERVICE_NO_LOGS_TO_PROCESS);
  }

  SCP_INFO(kCheckpointService, activity_id_,
           "Total log count in this checkpoint file: '%llu'",
           checkpoint_logs->size());

  // Unique wall-clock timestamp is used for checkpoint_id
  Timestamp current_clock =
      TimeProvider::GetUniqueWallTimestampInNanoseconds().count();
  checkpoint_id = current_clock;
  last_checkpoint_metadata.set_last_checkpoint_id(checkpoint_id);
  SCP_INFO(kCheckpointService, activity_id_,
           "Last checkpoint id set to '%llu'. This id will be persisted in "
           "last_checkpoint file",
           checkpoint_id);
  size_t current_buffer_offset = 0;
  size_t current_bytes_serialized = 0;
  execution_result = JournalSerialization::SerializeLastCheckpointMetadata(
      last_checkpoint_buffer, 0, last_checkpoint_metadata,
      current_bytes_serialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  last_checkpoint_buffer.length = current_bytes_serialized;

  current_buffer_offset = 0;
  current_bytes_serialized = 0;
  for (auto checkpoint_log = checkpoint_logs->begin();
       checkpoint_log != checkpoint_logs->end(); ++checkpoint_log) {
    auto successfully_written = false;
    auto buffer_offset_before_serialization = current_buffer_offset;

    while (!successfully_written) {
      auto execution_result = JournalSerialization::SerializeLogHeader(
          checkpoint_buffer, current_buffer_offset, current_clock,
          checkpoint_log->log_status, checkpoint_log->component_id,
          checkpoint_log->log_id, current_bytes_serialized);

      if (!execution_result.Successful()) {
        if (execution_result ==
            FailureExecutionResult(
                core::errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE)) {
          current_buffer_offset = buffer_offset_before_serialization;
          checkpoint_buffer.bytes->resize(2 * checkpoint_buffer.bytes->size());
          checkpoint_buffer.capacity = 2 * checkpoint_buffer.capacity;
          continue;
        }

        return execution_result;
      }

      successfully_written = true;
    }

    last_persisted_checkpoint_id_ = checkpoint_id;
    current_buffer_offset += current_bytes_serialized;
    current_bytes_serialized = 0;

    JournalLog journal_log;
    journal_log.set_log_body(checkpoint_log->bytes_buffer.bytes->data(),
                             checkpoint_log->bytes_buffer.length);

    successfully_written = false;
    buffer_offset_before_serialization = current_buffer_offset;

    while (!successfully_written) {
      execution_result = JournalSerialization::SerializeJournalLog(
          checkpoint_buffer, current_buffer_offset, journal_log,
          current_bytes_serialized);

      if (!execution_result.Successful()) {
        if (execution_result ==
            FailureExecutionResult(
                core::errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE)) {
          current_buffer_offset = buffer_offset_before_serialization;
          checkpoint_buffer.bytes->resize(2 * checkpoint_buffer.bytes->size());
          checkpoint_buffer.capacity = 2 * checkpoint_buffer.capacity;
          continue;
        }

        return execution_result;
      }
      successfully_written = true;
    }

    current_buffer_offset += current_bytes_serialized;
    current_bytes_serialized = 0;
  }

  checkpoint_buffer.length = current_buffer_offset;

  if (checkpoint_buffer.capacity - checkpoint_buffer.length <
      kBufferIncreaseThreshold) {
    checkpoint_buffer.bytes->resize(checkpoint_buffer.capacity +
                                    kBufferIncreaseThreshold);
    checkpoint_buffer.capacity += kBufferIncreaseThreshold;
  }

  checkpoint_metadata.set_last_processed_journal_id(last_processed_journal_id);
  current_bytes_serialized = 0;
  execution_result = JournalSerialization::SerializeCheckpointMetadata(
      checkpoint_buffer, current_buffer_offset, checkpoint_metadata,
      current_bytes_serialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  checkpoint_buffer.length += current_bytes_serialized;
  return execution_result;
}

ExecutionResult CheckpointService::WriteBlob(
    shared_ptr<BlobStorageClientInterface>& blob_storage_client,
    shared_ptr<string>& blob_name, shared_ptr<BytesBuffer>& bytes_buffer) {
  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context;
  put_blob_context.parent_activity_id = activity_id_;
  put_blob_context.correlation_id = activity_id_;
  put_blob_context.request = make_shared<PutBlobRequest>();
  put_blob_context.request->blob_name = blob_name;
  put_blob_context.request->buffer = bytes_buffer;
  put_blob_context.request->bucket_name = bucket_name_;

  promise<ExecutionResult> put_blob_execution_result;
  put_blob_context.callback =
      [&](AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) {
        put_blob_execution_result.set_value(put_blob_context.result);
      };

  auto execution_result = blob_storage_client->PutBlob(put_blob_context);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  return put_blob_execution_result.get_future().get();
}

ExecutionResult CheckpointService::Store(
    CheckpointId& checkpoint_id, BytesBuffer& last_checkpoint_buffer,
    BytesBuffer& checkpoint_buffer) noexcept {
  shared_ptr<BlobStorageClientInterface> blob_storage_client;
  auto execution_result =
      blob_storage_provider_->CreateBlobStorageClient(blob_storage_client);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  shared_ptr<string> checkpoint_blob_name;
  execution_result = JournalUtils::CreateCheckpointBlobName(
      partition_name_, checkpoint_id, checkpoint_blob_name);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  auto checkpoint_buffer_ptr = make_shared<BytesBuffer>(checkpoint_buffer);
  execution_result = WriteBlob(blob_storage_client, checkpoint_blob_name,
                               checkpoint_buffer_ptr);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  SCP_INFO(kCheckpointService, activity_id_,
           "Wrote Checkpoint file with file name : %s",
           checkpoint_blob_name->c_str());
  shared_ptr<string> last_checkpoint_blob_name =
      make_shared<string>(kLastCheckpointBlobName);
  shared_ptr<string> last_checkpoint_full_path;
  execution_result = JournalUtils::GetBlobFullPath(
      partition_name_, last_checkpoint_blob_name, last_checkpoint_full_path);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  auto last_checkpoint_buffer_ptr =
      make_shared<BytesBuffer>(last_checkpoint_buffer);
  return WriteBlob(blob_storage_client, last_checkpoint_full_path,
                   last_checkpoint_buffer_ptr);
}

ExecutionResult CheckpointService::Shutdown() noexcept {
  try {
    if (io_async_executor_) {
      io_async_executor_->Stop();
    }
  } catch (...) {
    // Ignore
  }
  try {
    if (async_executor_) {
      async_executor_->Stop();
    }
  } catch (...) {
    // Ignore
  }

  async_executor_ = nullptr;
  io_async_executor_ = nullptr;
  journal_service_ = nullptr;
  budget_key_provider_ = nullptr;
  transaction_command_serializer_ = nullptr;
  transaction_manager_ = nullptr;
  return SuccessExecutionResult();
}

ExecutionResultOr<CheckpointId>
CheckpointService::GetLastPersistedCheckpointId() noexcept {
  if (last_persisted_checkpoint_id_ == 0) {
    return FailureExecutionResult(
        core::errors::
            SC_PBS_CHECKPOINT_SERVICE_INVALID_LAST_PERSISTED_CHECKPOINT_ID);
  }
  return last_persisted_checkpoint_id_;
}

}  // namespace google::scp::pbs
