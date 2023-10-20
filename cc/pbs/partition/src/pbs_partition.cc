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

#include "pbs/partition/src/pbs_partition.h"

#include <memory>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/logger_interface.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "core/journal_service/src/journal_service.h"
#include "core/transaction_manager/src/transaction_manager.h"
#include "pbs/budget_key_provider/src/budget_key_provider.h"
#include "pbs/checkpoint_service/src/checkpoint_service.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/partition/src/error_codes.h"
#include "pbs/transactions/src/consume_budget_command.h"
#include "pbs/transactions/src/transaction_command_serializer.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionManagerStatusRequest;
using google::scp::core::GetTransactionManagerStatusResponse;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::JournalRecoverRequest;
using google::scp::core::JournalRecoverResponse;
using google::scp::core::JournalService;
using google::scp::core::JournalServiceInterface;
using google::scp::core::PartitionLoadUnloadState;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TransactionCommandSerializerInterface;
using google::scp::core::TransactionManager;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

static constexpr char kPBSPartition[] = "PBSPartition";

#define INIT_PBS_PARTITION_COMPONENT(component)                  \
  if (auto execution_result = component->Init();                 \
      !execution_result.Successful()) {                          \
    SCP_CRITICAL(kPBSPartition, partition_id_, execution_result, \
                 "PBS partition component '" #component          \
                 "' failed to initialize");                      \
    return execution_result;                                     \
  } else {                                                       \
    SCP_INFO(kPBSPartition, partition_id_,                       \
             "PBS partition component '" #component              \
             "'successfully initialized");                       \
  }

#define RUN_PBS_PARTITION_COMPONENT(component)                              \
  if (auto execution_result = component->Run();                             \
      !execution_result.Successful()) {                                     \
    SCP_CRITICAL(kPBSPartition, partition_id_, execution_result,            \
                 "PBS partition component '" #component "' failed to run"); \
    return execution_result;                                                \
  } else {                                                                  \
    SCP_INFO(kPBSPartition, partition_id_,                                  \
             "PBS partition component '" #component "'successfully ran");   \
  }

#define STOP_PBS_PARTITION_COMPONENT(component)                               \
  if (auto execution_result = component->Stop();                              \
      !execution_result.Successful()) {                                       \
    SCP_CRITICAL(kPBSPartition, partition_id_, execution_result,              \
                 "PBS partition component '" #component "' failed to stop");  \
    return execution_result;                                                  \
  } else {                                                                    \
    SCP_INFO(kPBSPartition, partition_id_,                                    \
             "PBS partition component '" #component "'successfully stopped"); \
  }

namespace google::scp::pbs {

PBSPartition::PBSPartition(
    const core::PartitionId& partition_id,
    const Dependencies& partition_dependencies,
    std::shared_ptr<std::string> partition_journal_bucket_name,
    size_t partition_transaction_manager_capacity)
    : partition_id_(partition_id),
      partition_state_(PartitionLoadUnloadState::Created),
      partition_journal_bucket_name_(partition_journal_bucket_name),
      partition_transaction_manager_capacity_(
          partition_transaction_manager_capacity),
      partition_dependencies_(partition_dependencies),
      requests_seen_count_(0) {}

ExecutionResult PBSPartition::Init() noexcept {
  auto current_state = partition_state_.load();
  if (current_state != PartitionLoadUnloadState::Created) {
    SCP_INFO(kPBSPartition, partition_id_,
             "Cannot initialize partition at this state. Current State is %llu",
             static_cast<uint64_t>(current_state));
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_CANNOT_INITIALIZE);
  }

  std::shared_ptr<std::string> partition_id_str =
      std::make_shared<std::string>(ToString(partition_id_));

  journal_service_ = std::make_shared<JournalService>(
      partition_journal_bucket_name_, partition_id_str,
      partition_dependencies_.async_executor,
      partition_dependencies_.blob_store_provider,
      partition_dependencies_.metric_client,
      partition_dependencies_.config_provider);

  checkpoint_service_ = std::make_shared<CheckpointService>(
      partition_journal_bucket_name_, partition_id_str,
      partition_dependencies_.metric_client,
      partition_dependencies_.config_provider, journal_service_,
      partition_dependencies_.blob_store_provider_for_checkpoints);

  budget_key_provider_ = std::make_shared<BudgetKeyProvider>(
      partition_dependencies_.async_executor, journal_service_,
      partition_dependencies_.nosql_database_provider_for_background_operations,
      partition_dependencies_.nosql_database_provider_for_live_traffic,
      partition_dependencies_.metric_client,
      partition_dependencies_.config_provider, partition_id_);

  std::shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer =
          std::make_shared<TransactionCommandSerializer>(
              partition_dependencies_.async_executor, budget_key_provider_);

  transaction_manager_ = std::make_shared<TransactionManager>(
      partition_dependencies_.async_executor, transaction_command_serializer,
      journal_service_, partition_dependencies_.remote_transaction_manager,
      partition_transaction_manager_capacity_,
      partition_dependencies_.metric_client,
      partition_dependencies_.config_provider, partition_id_);

  INIT_PBS_PARTITION_COMPONENT(journal_service_);
  INIT_PBS_PARTITION_COMPONENT(budget_key_provider_);
  INIT_PBS_PARTITION_COMPONENT(transaction_manager_);
  INIT_PBS_PARTITION_COMPONENT(checkpoint_service_);

  current_state = PartitionLoadUnloadState::Created;
  if (!partition_state_.compare_exchange_strong(
          current_state, PartitionLoadUnloadState::Initialized)) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_CANNOT_INITIALIZE);
  }

  SCP_INFO(kPBSPartition, partition_id_, "Initialized Partition with ID: %s",
           partition_id_str->c_str());

  return SuccessExecutionResult();
}

ExecutionResult PBSPartition::RecoverPartition() {
  SCP_INFO(kPBSPartition, partition_id_, "Starting log recovery");

  std::atomic<bool> recovery_completed = false;
  std::atomic<bool> recovery_failed = false;
  AsyncContext<JournalRecoverRequest, JournalRecoverResponse> recovery_context;
  recovery_context.request = std::make_shared<JournalRecoverRequest>();
  auto activity_id = Uuid::GenerateUuid();
  recovery_context.parent_activity_id = activity_id;
  recovery_context.correlation_id = activity_id;
  recovery_context.callback =
      [&](AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
              recovery_context) {
        if (!recovery_context.result.Successful()) {
          SCP_CRITICAL(kPBSPartition, partition_id_, recovery_context.result,
                       "Log recovery failed.");
          recovery_failed = true;
        }
        recovery_completed = true;
      };

  // Recovering the service
  auto start_timestamp = TimeProvider::GetSteadyTimestampInNanoseconds();

  // Recovery metrics needs to be separately Run because the journal_service_ is
  // not yet Run().
  RETURN_IF_FAILURE(journal_service_->RunRecoveryMetrics());
  RETURN_IF_FAILURE(journal_service_->Recover(recovery_context));

  while (!recovery_completed) {
    auto time_elapsed = duration_cast<milliseconds>(
        TimeProvider::GetSteadyTimestampInNanoseconds() - start_timestamp);
    SCP_INFO(
        kPBSPartition, partition_id_,
        "Waiting on log recovery to complete. Time elapsed so far (ms): '%llu'",
        time_elapsed.count());
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  RETURN_IF_FAILURE(journal_service_->StopRecoveryMetrics());

  if (recovery_failed) {
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_RECOVERY_FAILED);
  }

  SCP_INFO(kPBSPartition, partition_id_, "Done with log recovery");

  return SuccessExecutionResult();
}

ExecutionResult PBSPartition::Load() noexcept {
  auto current_state = PartitionLoadUnloadState::Initialized;
  if (!partition_state_.compare_exchange_strong(
          current_state, PartitionLoadUnloadState::Loading)) {
    SCP_INFO(kPBSPartition, partition_id_,
             "Cannot load partition at this state. Current State is %llu",
             static_cast<uint64_t>(current_state));
    return FailureExecutionResult(core::errors::SC_PBS_PARTITION_CANNOT_LOAD);
  }

  SCP_INFO(kPBSPartition, partition_id_, "Loading Partition with ID: %s",
           ToString(partition_id_).c_str());

  RETURN_IF_FAILURE(RecoverPartition());

  RUN_PBS_PARTITION_COMPONENT(journal_service_);
  RUN_PBS_PARTITION_COMPONENT(budget_key_provider_);
  RUN_PBS_PARTITION_COMPONENT(transaction_manager_);
  RUN_PBS_PARTITION_COMPONENT(checkpoint_service_);

  current_state = PartitionLoadUnloadState::Loading;
  if (!partition_state_.compare_exchange_strong(
          current_state, PartitionLoadUnloadState::Loaded)) {
    SCP_INFO(kPBSPartition, partition_id_,
             "Cannot load partition at this state. Current State is %llu",
             static_cast<uint64_t>(current_state));
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_INVALID_PARTITON_STATE);
  }

  SCP_INFO(kPBSPartition, partition_id_, "Loaded Partition with ID: %s",
           ToString(partition_id_).c_str());

  return SuccessExecutionResult();
}

ExecutionResult PBSPartition::Unload() noexcept {
  auto current_state = PartitionLoadUnloadState::Loaded;
  if (!partition_state_.compare_exchange_strong(
          current_state, PartitionLoadUnloadState::Unloading)) {
    SCP_INFO(kPBSPartition, partition_id_,
             "Cannot load partition at this state. Current State is %llu",
             static_cast<uint64_t>(current_state));
    return FailureExecutionResult(core::errors::SC_PBS_PARTITION_CANNOT_UNLOAD);
  }

  SCP_INFO(kPBSPartition, partition_id_, "Unloading Partition with ID: %s",
           ToString(partition_id_).c_str());

  STOP_PBS_PARTITION_COMPONENT(checkpoint_service_);
  STOP_PBS_PARTITION_COMPONENT(transaction_manager_);
  STOP_PBS_PARTITION_COMPONENT(budget_key_provider_);
  STOP_PBS_PARTITION_COMPONENT(journal_service_);

  current_state = PartitionLoadUnloadState::Unloading;
  if (!partition_state_.compare_exchange_strong(
          current_state, PartitionLoadUnloadState::Unloaded)) {
    SCP_INFO(kPBSPartition, partition_id_,
             "Cannot load partition at this state. Current State is %llu",
             static_cast<uint64_t>(current_state));
    return FailureExecutionResult(
        core::errors::SC_PBS_PARTITION_INVALID_PARTITON_STATE);
  }

  SCP_INFO(kPBSPartition, partition_id_, "Unloaded Partition with ID: %s",
           ToString(partition_id_).c_str());

  return SuccessExecutionResult();
}

PartitionLoadUnloadState PBSPartition::GetPartitionState() noexcept {
  return partition_state_;
}

void PBSPartition::IncrementRequestCount() {
  requests_seen_count_.fetch_add(1, std::memory_order::memory_order_relaxed);
  // Emit the counter every 1000 requests.
  // TODO: Convert this to a metric.
  if (requests_seen_count_.load() % 1000 == 0) {
    SCP_INFO(kPBSPartition, partition_id_,
             "Partition with ID: '%s' has recevied '%llu' requests so far.",
             ToString(partition_id_).c_str(), requests_seen_count_.load());
  }
}

ExecutionResult PBSPartition::ExecuteRequest(
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        context) noexcept {
  if (partition_state_ != PartitionLoadUnloadState::Loaded) {
    return RetryExecutionResult(core::errors::SC_PBS_PARTITION_NOT_LOADED);
  }

  IncrementRequestCount();

  return transaction_manager_->ExecutePhase(context);
}

ExecutionResult PBSPartition::ExecuteRequest(
    AsyncContext<TransactionRequest, TransactionResponse>& context) noexcept {
  if (partition_state_ != PartitionLoadUnloadState::Loaded) {
    return RetryExecutionResult(core::errors::SC_PBS_PARTITION_NOT_LOADED);
  }

  IncrementRequestCount();

  // Set budget command dependencies
  for (auto& command : context.request->commands) {
    auto consume_budget_command =
        std::dynamic_pointer_cast<ConsumeBudgetCommand>(command);
    if (!consume_budget_command) {
      return FailureExecutionResult(
          core::errors::SC_PBS_PARTITION_INVALID_TRANSACTION);
    }
    consume_budget_command->SetUpCommandExecutionDependencies(
        budget_key_provider_, partition_dependencies_.async_executor);
  }

  return transaction_manager_->Execute(context);
}

ExecutionResult PBSPartition::GetTransactionStatus(
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
        context) noexcept {
  if (partition_state_ != PartitionLoadUnloadState::Loaded) {
    return RetryExecutionResult(core::errors::SC_PBS_PARTITION_NOT_LOADED);
  }

  IncrementRequestCount();

  return transaction_manager_->GetTransactionStatus(context);
}

ExecutionResult PBSPartition::GetTransactionManagerStatus(
    const GetTransactionManagerStatusRequest& request,
    GetTransactionManagerStatusResponse& response) noexcept {
  if (partition_state_ != PartitionLoadUnloadState::Loaded) {
    return RetryExecutionResult(core::errors::SC_PBS_PARTITION_NOT_LOADED);
  }

  IncrementRequestCount();

  return transaction_manager_->GetTransactionManagerStatus(request, response);
}

}  // namespace google::scp::pbs
