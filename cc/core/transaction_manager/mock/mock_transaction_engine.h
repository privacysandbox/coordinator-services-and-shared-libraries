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

#pragma once

#include <functional>
#include <memory>
#include <string>

#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/async_executor_interface.h"
#include "core/transaction_manager/src/transaction_engine.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

namespace google::scp::core::transaction_manager::mock {
class MockTransactionEngine : public core::TransactionEngine {
 public:
  MockTransactionEngine(
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<TransactionCommandSerializerInterface>&
          transaction_command_serializer,
      std::shared_ptr<JournalServiceInterface>& journal_service,
      std::shared_ptr<RemoteTransactionManagerInterface>
          remote_transaction_manager,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client)
      : core::TransactionEngine(
            async_executor, transaction_command_serializer, journal_service,
            remote_transaction_manager, metric_client,
            std::make_shared<config_provider::mock::MockConfigProvider>()) {}

  MockTransactionEngine(
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<TransactionCommandSerializerInterface>&
          transaction_command_serializer,
      std::shared_ptr<JournalServiceInterface>& journal_service,
      std::shared_ptr<RemoteTransactionManagerInterface>
          remote_transaction_manager,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      std::shared_ptr<ConfigProviderInterface> config_provider)
      : core::TransactionEngine(async_executor, transaction_command_serializer,
                                journal_service, remote_transaction_manager,
                                metric_client, config_provider) {}

  MockTransactionEngine(
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<TransactionCommandSerializerInterface>&
          transaction_command_serializer,
      std::shared_ptr<JournalServiceInterface>& journal_service,
      std::shared_ptr<transaction_manager::TransactionPhaseManagerInterface>
          transaction_phase_manager,
      std::shared_ptr<RemoteTransactionManagerInterface>
          remote_transaction_manager,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      size_t transaction_engine_cache_lifetime_seconds)
      : core::TransactionEngine(
            async_executor, transaction_command_serializer, journal_service,
            transaction_phase_manager, remote_transaction_manager,
            metric_client,
            std::make_shared<config_provider::mock::MockConfigProvider>(),
            transaction_engine_cache_lifetime_seconds) {}

  ExecutionResult Init() noexcept {
    if (init_mock) {
      return init_mock();
    }

    return core::TransactionEngine::Init();
  }

  ExecutionResult Run() noexcept {
    if (run_mock) {
      return run_mock();
    }

    return core::TransactionEngine::Run();
  }

  ExecutionResult Stop() noexcept {
    if (stop_mock) {
      return stop_mock();
    }

    return core::TransactionEngine::Stop();
  }

  ExecutionResult LockRemotelyCoordinatedTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (lock_remotely_coordinated_transaction_mock) {
      return lock_remotely_coordinated_transaction_mock(transaction);
    }
    return TransactionEngine::LockRemotelyCoordinatedTransaction(transaction);
  }

  ExecutionResult UnlockRemotelyCoordinatedTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (unlock_remotely_coordinated_transaction_mock) {
      return unlock_remotely_coordinated_transaction_mock(transaction);
    }
    return TransactionEngine::UnlockRemotelyCoordinatedTransaction(transaction);
  }

  ExecutionResult ResolveTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    return TransactionEngine::ResolveTransaction(transaction);
  }

  bool LocalAndRemoteTransactionsInSync(
      transaction_manager::TransactionPhase transaction_phase,
      transaction_manager::TransactionPhase remote_transaction_phase) noexcept
      override {
    return TransactionEngine::LocalAndRemoteTransactionsInSync(
        transaction_phase, remote_transaction_phase);
  }

  ExecutionResult RollForwardLocalTransaction(
      std::shared_ptr<Transaction>& transaction,
      TransactionExecutionPhase next_phase,
      bool next_phase_has_failure) noexcept override {
    if (roll_forward_local_transaction_mock) {
      return roll_forward_local_transaction_mock(transaction, next_phase,
                                                 next_phase_has_failure);
    }

    return TransactionEngine::RollForwardLocalTransaction(
        transaction, next_phase, next_phase_has_failure);
  }

  ExecutionResult RollForwardLocalAndRemoteTransactions(
      std::shared_ptr<Transaction>& transaction,
      Timestamp last_execution_timestamp_remote) noexcept override {
    if (roll_forward_local_and_remote_transactions_mock) {
      return roll_forward_local_and_remote_transactions_mock(
          transaction, last_execution_timestamp_remote);
    }

    return TransactionEngine::RollForwardLocalAndRemoteTransactions(
        transaction, last_execution_timestamp_remote);
  }

  void OnRollForwardRemoteTransactionCallback(
      std::shared_ptr<Transaction>& transaction,
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
          remote_transaction_context) noexcept override {
    return TransactionEngine::OnRollForwardRemoteTransactionCallback(
        transaction, remote_transaction_context);
  }

  void OnGetRemoteTransactionStatusCallback(
      std::shared_ptr<Transaction>& transaction,
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept override {
    return TransactionEngine::OnGetRemoteTransactionStatusCallback(
        transaction, get_transaction_status_context);
  }

  void OnRemoteTransactionNotFound(
      std::shared_ptr<Transaction>& transaction,
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept override {
    if (on_remote_transaction_not_found) {
      on_remote_transaction_not_found(transaction,
                                      get_transaction_status_context);
      return;
    }
    TransactionEngine::OnRemoteTransactionNotFound(
        transaction, get_transaction_status_context);

    return;
  }

  ExecutionResult InitializeTransaction(
      AsyncContext<TransactionRequest, TransactionResponse>&
          transaction_context,
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (initialize_transaction_mock) {
      return initialize_transaction_mock(transaction_context, transaction);
    }

    return core::TransactionEngine::InitializeTransaction(transaction_context,
                                                          transaction);
  }

  ExecutionResult SerializeTransaction(
      std::shared_ptr<Transaction>& transaction,
      BytesBuffer& output_buffer) noexcept override {
    return core::TransactionEngine::SerializeTransaction(transaction,
                                                         output_buffer);
  }

  ExecutionResult SerializeState(std::shared_ptr<Transaction>& transaction,
                                 BytesBuffer& output_buffer) noexcept override {
    return core::TransactionEngine::SerializeState(transaction, output_buffer);
  }

  void ProceedToNextPhase(
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (proceed_to_next_phase_mock) {
      proceed_to_next_phase_mock(current_phase, transaction);
      return;
    }
    core::TransactionEngine::ProceedToNextPhase(current_phase, transaction);
  }

  void ProceedToNextPhaseAfterRecovery(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    core::TransactionEngine::ProceedToNextPhaseAfterRecovery(transaction);
  }

  void BeginTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (begin_transaction_mock) {
      begin_transaction_mock(transaction);
      return;
    }
    core::TransactionEngine::BeginTransaction(transaction);
  }

  void PrepareTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (prepare_transaction_mock) {
      prepare_transaction_mock(transaction);
      return;
    }
    core::TransactionEngine::PrepareTransaction(transaction);
  }

  void CommitTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (commit_transaction_mock) {
      commit_transaction_mock(transaction);
      return;
    }
    core::TransactionEngine::CommitTransaction(transaction);
  }

  void CommitNotifyTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (commit_notify_transaction_mock) {
      commit_notify_transaction_mock(transaction);
      return;
    }
    core::TransactionEngine::CommitNotifyTransaction(transaction);
  }

  void CommittedTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (committed_transaction_mock) {
      committed_transaction_mock(transaction);
      return;
    }
    core::TransactionEngine::CommittedTransaction(transaction);
  }

  void AbortNotifyTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (abort_notify_transaction_mock) {
      abort_notify_transaction_mock(transaction);
      return;
    }
    core::TransactionEngine::AbortNotifyTransaction(transaction);
  }

  void AbortedTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (aborted_transaction_mock) {
      aborted_transaction_mock(transaction);
      return;
    }
    core::TransactionEngine::AbortedTransaction(transaction);
  }

  void EndTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (end_transaction_mock) {
      end_transaction_mock(transaction);
      return;
    }
    core::TransactionEngine::EndTransaction(transaction);
  }

  void UnknownStateTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (unknown_state_transaction_mock) {
      unknown_state_transaction_mock(transaction);
      return;
    }
    core::TransactionEngine::UnknownStateTransaction(transaction);
  }

  void ExecuteDistributedPhase(
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (execute_distributed_phase_mock) {
      execute_distributed_phase_mock(current_phase, transaction);
      return;
    }
    core::TransactionEngine::ExecuteDistributedPhase(current_phase,
                                                     transaction);
  }

  ExecutionResult DispatchDistributedCommand(
      size_t command_index, transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<TransactionCommand>& command,
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (dispatch_distributed_command_mock) {
      return dispatch_distributed_command_mock(current_phase, command,
                                               transaction);
    }
    return core::TransactionEngine::DispatchDistributedCommand(
        command_index, current_phase, command, transaction);
  }

  void OnPhaseCallback(
      size_t command_index, transaction_manager::TransactionPhase current_phase,
      ExecutionResult& execution_result,
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (on_phase_callback_mock) {
      on_phase_callback_mock(current_phase, execution_result, transaction);
      return;
    }
    core::TransactionEngine::OnPhaseCallback(command_index, current_phase,
                                             execution_result, transaction);
  }

  transaction_manager::proto::TransactionPhase ConvertPhaseToProtoPhase(
      transaction_manager::TransactionPhase phase) noexcept override {
    return TransactionEngine::ConvertPhaseToProtoPhase(phase);
  }

  transaction_manager::TransactionPhase ConvertProtoPhaseToPhase(
      transaction_manager::proto::TransactionPhase phase) noexcept override {
    return TransactionEngine::ConvertProtoPhaseToPhase(phase);
  }

  ExecutionResult OnJournalServiceRecoverCallback(
      const std::shared_ptr<core::BytesBuffer>& bytes_buffer,
      const core::common::Uuid& activity_id) noexcept override {
    return TransactionEngine::OnJournalServiceRecoverCallback(bytes_buffer,
                                                              activity_id);
  }

  ExecutionResult LogTransactionAndProceedToNextPhase(
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (log_transaction_and_proceed_to_next_phase_mock) {
      return log_transaction_and_proceed_to_next_phase_mock(current_phase,
                                                            transaction);
    }

    return TransactionEngine::LogTransactionAndProceedToNextPhase(current_phase,
                                                                  transaction);
  }

  ExecutionResult LogStateAndProceedToNextPhase(
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (log_state_and_proceed_to_next_phase_mock) {
      return log_state_and_proceed_to_next_phase_mock(current_phase,
                                                      transaction);
    }

    return TransactionEngine::LogStateAndProceedToNextPhase(current_phase,
                                                            transaction);
  }

  ExecutionResult LogStateAndExecuteDistributedPhase(
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept override {
    if (log_state_and_execute_distributed_phase_mock) {
      return log_state_and_execute_distributed_phase_mock(current_phase,
                                                          transaction);
    }

    return TransactionEngine::LogStateAndExecuteDistributedPhase(current_phase,
                                                                 transaction);
  }

  virtual void OnLogTransactionCallback(
      AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context,
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept {
    return TransactionEngine::OnLogTransactionCallback(
        journal_log_context, current_phase, transaction);
  }

  virtual void OnLogStateAndProceedToNextPhaseCallback(
      AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context,
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept {
    return TransactionEngine::OnLogStateAndProceedToNextPhaseCallback(
        journal_log_context, current_phase, transaction);
  }

  void OnBeforeGarbageCollection(
      common::Uuid& transaction_id, std::shared_ptr<Transaction>& transaction,
      std::function<void(bool)> should_delete_entry) noexcept override {
    if (on_before_garbage_collection_pre_caller) {
      on_before_garbage_collection_pre_caller(transaction_id, transaction,
                                              should_delete_entry);
      TransactionEngine::OnBeforeGarbageCollection(transaction_id, transaction,
                                                   should_delete_entry);
      return;
    }

    if (on_before_garbage_collection_mock) {
      on_before_garbage_collection_pre_caller(transaction_id, transaction,
                                              should_delete_entry);
      return;
    }

    TransactionEngine::OnBeforeGarbageCollection(transaction_id, transaction,
                                                 should_delete_entry);
  }

  TransactionExecutionPhase ToTransactionExecutionPhase(
      transaction_manager::TransactionPhase phase) noexcept override {
    return TransactionEngine::ToTransactionExecutionPhase(phase);
  }

  transaction_manager::TransactionPhase ToTransactionPhase(
      TransactionExecutionPhase phase) noexcept override {
    return TransactionEngine::ToTransactionPhase(phase);
  }

  bool CanCancel(transaction_manager::TransactionPhase
                     transaction_phase) noexcept override {
    return TransactionEngine::CanCancel(transaction_phase);
  }

  ExecutionResult ExecutePhaseInternal(
      std::shared_ptr<Transaction>& transaction,
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
          transaction_phase_context) noexcept override {
    if (execute_phase_internal_mock) {
      return execute_phase_internal_mock(transaction,
                                         transaction_phase_context);
    }

    return TransactionEngine::ExecutePhaseInternal(transaction,
                                                   transaction_phase_context);
  }

  auto& GetActiveTransactionsMap() {
    return core::TransactionEngine::active_transactions_map_;
  }

  static std::string TransactionExecutionPhaseToString(
      TransactionExecutionPhase phase) {
    return TransactionEngine::TransactionExecutionPhaseToString(phase);
  }

  static std::string TransactionPhaseToString(TransactionPhase phase) {
    return TransactionEngine::TransactionPhaseToString(phase);
  }

  std::function<ExecutionResult()> init_mock;
  std::function<ExecutionResult()> run_mock;
  std::function<ExecutionResult()> stop_mock;

  std::function<ExecutionResult(
      AsyncContext<TransactionRequest, TransactionResponse>&,
      std::shared_ptr<Transaction>&)>
      initialize_transaction_mock;

  std::function<void(transaction_manager::TransactionPhase,
                     std::shared_ptr<Transaction>&)>
      proceed_to_next_phase_mock;

  std::function<core::ExecutionResult(transaction_manager::TransactionPhase,
                                      std::shared_ptr<Transaction>&)>
      log_transaction_and_proceed_to_next_phase_mock;

  std::function<core::ExecutionResult(transaction_manager::TransactionPhase,
                                      std::shared_ptr<Transaction>&)>
      log_state_and_proceed_to_next_phase_mock;

  std::function<core::ExecutionResult(transaction_manager::TransactionPhase,
                                      std::shared_ptr<Transaction>&)>
      log_state_and_execute_distributed_phase_mock;

  std::function<void(transaction_manager::TransactionPhase,
                     std::shared_ptr<Transaction>&)>
      execute_distributed_phase_mock;

  std::function<ExecutionResult(transaction_manager::TransactionPhase,
                                std::shared_ptr<TransactionCommand>&,
                                std::shared_ptr<Transaction>&)>
      dispatch_distributed_command_mock;

  std::function<void(transaction_manager::TransactionPhase, ExecutionResult&,
                     std::shared_ptr<Transaction>&)>
      on_phase_callback_mock;

  std::function<void(std::shared_ptr<Transaction>&)> begin_transaction_mock;
  std::function<void(std::shared_ptr<Transaction>&)> prepare_transaction_mock;
  std::function<void(std::shared_ptr<Transaction>&)> commit_transaction_mock;
  std::function<void(std::shared_ptr<Transaction>&)>
      commit_notify_transaction_mock;
  std::function<void(std::shared_ptr<Transaction>&)> committed_transaction_mock;
  std::function<void(std::shared_ptr<Transaction>&)>
      abort_notify_transaction_mock;
  std::function<void(std::shared_ptr<Transaction>&)> aborted_transaction_mock;
  std::function<void(std::shared_ptr<Transaction>&)> end_transaction_mock;
  std::function<void(std::shared_ptr<Transaction>&)>
      unknown_state_transaction_mock;
  std::function<void(common::Uuid&, std::shared_ptr<Transaction>&,
                     std::function<void(bool)>&)>
      on_before_garbage_collection_mock;
  std::function<void(common::Uuid&, std::shared_ptr<Transaction>&,
                     std::function<void(bool)>&)>
      on_before_garbage_collection_pre_caller;
  std::function<void(
      std::shared_ptr<Transaction>&,
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&)>
      on_remote_transaction_not_found;
  std::function<ExecutionResult(std::shared_ptr<Transaction>&)>
      lock_remotely_coordinated_transaction_mock;
  std::function<ExecutionResult(std::shared_ptr<Transaction>&)>
      unlock_remotely_coordinated_transaction_mock;

  std::function<ExecutionResult(std::shared_ptr<Transaction>&,
                                TransactionExecutionPhase, bool)>
      roll_forward_local_transaction_mock;

  std::function<ExecutionResult(std::shared_ptr<Transaction>&, Timestamp)>
      roll_forward_local_and_remote_transactions_mock;

  std::function<ExecutionResult(
      std::shared_ptr<Transaction>&,
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&)>
      execute_phase_internal_mock;

  std::shared_ptr<RemoteTransactionManagerInterface>
      remote_transaction_manager = nullptr;
};
}  // namespace google::scp::core::transaction_manager::mock
