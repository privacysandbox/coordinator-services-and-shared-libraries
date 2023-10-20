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

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <string>

#include "core/common/auto_expiry_concurrent_map/src/auto_expiry_concurrent_map.h"
#include "core/common/concurrent_queue/src/concurrent_queue.h"
#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/src/config_provider.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/checkpoint_service_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "core/transaction_manager/interface/transaction_engine_interface.h"
#include "core/transaction_manager/interface/transaction_phase_manager_interface.h"
#include "core/transaction_manager/src/proto/transaction_engine.pb.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

#include "error_codes.h"

// TODO: Make the retry strategy configurable.
static constexpr google::scp::core::TimeDuration
    kTransactionManagerRetryStrategyDelayMs = 31;
static constexpr size_t kTransactionManagerRetryStrategyTotalRetries = 12;
static constexpr size_t kTransactionEngineCacheLifetimeSeconds = 30;
static constexpr size_t kTransactionTimeoutSeconds = 300;

namespace google::scp::core {
/**
 * @brief Is used as a common context between different phases of each
 * transaction. This object will be cached into a map and transaction engine can
 * access it at any time.
 */
struct Transaction : public LoadableObject {
  Transaction()
      : id(common::kZeroUuid),
        current_phase(transaction_manager::TransactionPhase::NotStarted),
        current_phase_execution_result(core::SuccessExecutionResult()),
        transaction_execution_result(core::FailureExecutionResult(
            errors::SC_TRANSACTION_MANAGER_NOT_FINISHED)),
        pending_callbacks(0),
        transaction_failed(false),
        current_phase_failed(false),
        current_phase_failed_command_indices(INT32_MAX),
        last_execution_timestamp(0),
        is_coordinated_remotely(false),
        is_waiting_for_remote(false) {}

  /// The current transaction id.
  common::Uuid id;

  /// The original context of the transaction.
  AsyncContext<TransactionRequest, TransactionResponse> context;

  /// The current phase of the transaction. This is the transaction phase that
  /// is to be executed on the transaction.
  std::atomic<transaction_manager::TransactionPhase> current_phase;

  /// The current phase execution result of the transaction.
  ExecutionResult current_phase_execution_result;

  /// The execution result of the transaction.
  ExecutionResult transaction_execution_result;

  /// Number of pending dispatched commands at the current phase.
  std::atomic<size_t> pending_callbacks;

  /// Indicates whether the transaction failed.
  std::atomic<bool> transaction_failed;

  /// Indicates whether the current phase failed.
  std::atomic<bool> current_phase_failed;

  /// The commands that failed in the current phase.
  common::ConcurrentQueue<size_t> current_phase_failed_command_indices;

  /// Last execution timestamp of any phases of the current transaction to
  /// support optimistic concurrency. This timestamp needs to be wall-clock
  /// timestamp.
  Timestamp last_execution_timestamp;

  /// Indicates whether the transaction is coordinated by a remote transaction
  /// manager.
  bool is_coordinated_remotely;

  /// Indicates whether the transaction is waiting for a remote command from
  /// either remote transaction manager, or remote PBS instance in the case of
  /// transaction resolution after transaction expiry.
  /// This boolean is used for synchronization between transaction phase
  /// execution requests from remote and internal activites on a
  /// transaction.
  std::atomic<bool> is_waiting_for_remote;

  /// The context of the remote phase operation.
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      remote_phase_context;

  /// The secret of the transaction execution.
  std::shared_ptr<std::string> transaction_secret;

  /// The origin of the transaction execution.
  std::shared_ptr<std::string> transaction_origin;

  /// Transaction expiration time
  Timestamp expiration_time;

  /// Indicates if the transaction is blocked and needs manual resolution.
  bool blocked = false;

  /// Indicates whether the transaction is expired.
  bool IsExpired() {
    Timestamp current_time =
        common::TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();

    return expiration_time < current_time;
  }
};

/*! @copydoc TransactionEngineInterface
 */
class TransactionEngine
    : public transaction_manager::TransactionEngineInterface {
 public:
  TransactionEngine(
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<TransactionCommandSerializerInterface>&
          transaction_command_serializer,
      std::shared_ptr<JournalServiceInterface>& journal_service,
      std::shared_ptr<RemoteTransactionManagerInterface>&
          remote_transaction_manager,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      std::shared_ptr<ConfigProviderInterface> config_provider);

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ~TransactionEngine();

  ExecutionResult Execute(AsyncContext<TransactionRequest, TransactionResponse>&
                              transaction_context) noexcept override;

  ExecutionResult ExecutePhase(
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
          transaction_phase_context) noexcept override;

  ExecutionResult Checkpoint(std::shared_ptr<std::list<CheckpointLog>>&
                                 checkpoint_logs) noexcept override;

  ExecutionResult GetTransactionStatus(
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept override;

  size_t GetPendingTransactionCount() noexcept override;

 protected:
  TransactionEngine(
      std::shared_ptr<AsyncExecutorInterface>& async_executor,
      std::shared_ptr<TransactionCommandSerializerInterface>&
          transaction_command_serializer,
      std::shared_ptr<JournalServiceInterface>& journal_service,
      std::shared_ptr<transaction_manager::TransactionPhaseManagerInterface>
          transaction_phase_manager,
      std::shared_ptr<RemoteTransactionManagerInterface>
          remote_transaction_manager,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      std::shared_ptr<ConfigProviderInterface> config_provider,
      size_t transaction_engine_cache_lifetime_seconds =
          kTransactionEngineCacheLifetimeSeconds)
      : active_transactions_count_(0),
        active_transactions_map_(
            transaction_engine_cache_lifetime_seconds,
            false /* extend_entry_lifetime_on_access */,
            false /* block_entry_while_eviction */,
            std::bind(&TransactionEngine::OnBeforeGarbageCollection, this,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3),
            async_executor),
        transaction_phase_manager_(transaction_phase_manager),
        remote_transaction_manager_(remote_transaction_manager),
        journal_service_(journal_service),
        async_executor_(async_executor),
        transaction_command_serializer_(transaction_command_serializer),
        operation_dispatcher_(
            async_executor, core::common::RetryStrategy(
                                core::common::RetryStrategyType::Exponential,
                                kTransactionManagerRetryStrategyDelayMs,
                                kTransactionManagerRetryStrategyTotalRetries)),
        metric_client_(metric_client),
        transaction_engine_cache_lifetime_seconds_(
            transaction_engine_cache_lifetime_seconds),
        config_provider_(config_provider),
        skip_log_recovery_failures_(false),
        transaction_timeout_in_seconds_(kTransactionTimeoutSeconds),
        transaction_resolution_with_remote_enabled_(true),
        activity_id_(core::common::Uuid::GenerateUuid()) {}

  /**
   * @brief Locks a local transaction that is being managed remotely. While the
   * transaction is locked, only one writer is allowed to change the
   * transaction state.
   *
   * @param transaction The transaction object to be locked.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult LockRemotelyCoordinatedTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Unlocks a local transaction that is being managed remotely.
   *
   * @param transaction The transaction object to be unlocked.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult UnlockRemotelyCoordinatedTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Is Called right before the map garbage collector is trying to remove
   * the element from the map.
   *
   * @param transaction_id The id of the transaction that is being removed.
   * @param transaction The transaction that is being removed.
   * @param should_delete_entry Callback to allow/deny the garbage collection
   * request.
   */
  virtual void OnBeforeGarbageCollection(
      common::Uuid& transaction_id, std::shared_ptr<Transaction>& transaction,
      std::function<void(bool)> should_delete_entry) noexcept;

  /**
   * @brief Is called when the garbage collector identifies an expired
   * transaction whether it is local or a remotely managed transaction.
   * Resolving a transaction is the operation of getting consensus between the
   * local transaction manager and the remote transaction manager to finalize a
   * transaction. To resolve a transaction, both transaction managers will
   * inquiry the status of the transaction id. Whichever transaction that is
   * behind will be rolled forward to the target transaction, and finally both
   * transactions will roll forward together.
   *
   * NOTE: If transaction with ID already initialized, the returned transaction
   * object contains the already initialized transaction object.
   *
   * @param transaction The transaction object to be resolved.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ResolveTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Returns true if the local transaction phase is in sync with the
   * remote instance of the transaction. This is crucial for transaction
   * resolution. If transactions are out of sync, manual recovery must be done.
   *
   * @param transaction_phase The local transaction phase.
   * @param remote_transaction_phase The remote transaction phase.
   * @return true if the local transaction phase is in sync with the
   * remote instance of the transaction
   * @return false if the local transaction phase is not in sync with the
   * remote instance of the transaction
   */
  virtual bool LocalAndRemoteTransactionsInSync(
      transaction_manager::TransactionPhase transaction_phase,
      transaction_manager::TransactionPhase remote_transaction_phase) noexcept;

  /**
   * @brief Rolls forward the local transaction 1 phase ahead.
   *
   * @param transaction The local transaction to be rolled forward.
   * @param next_phase The next phase to be rolled forward to.
   * @param next_phase_has_failure If the next targeted phase has failures.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult RollForwardLocalTransaction(
      std::shared_ptr<Transaction>& transaction,
      TransactionExecutionPhase next_phase,
      bool next_phase_has_failure) noexcept;

  /**
   * @brief Rolls forward both local and remote transactions. This happens, when
   * both of the remote and local transactions are in the same phase.
   *
   * @param transaction The current transaction to be rolled forward.
   * @param last_execution_timestamp_remote The last execution timestamp of the
   * remote transaction.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult RollForwardLocalAndRemoteTransactions(
      std::shared_ptr<Transaction>& transaction,
      Timestamp last_execution_timestamp_remote) noexcept;

  /**
   * @brief Is called when the remote transaction roll forward request is
   * completed.
   *
   * @param transaction The local instance of the transaction to be rolled
   * forward remotely.
   * @param remote_transaction_context The remote transaction context of the
   * operation.
   */
  virtual void OnRollForwardRemoteTransactionCallback(
      std::shared_ptr<Transaction>& transaction,
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
          remote_transaction_context) noexcept;

  /**
   * @brief Is called back when the remote transaction status call completes.
   *
   * @param transaction The expired transaction that its state needs to be
   * resolved.
   * @param get_transaction_status_context The get transaction status context.
   */
  virtual void OnGetRemoteTransactionStatusCallback(
      std::shared_ptr<Transaction>& transaction,
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept;

  /**
   * @brief Is called when the remote transaction is not found.
   *
   * @param transaction The local instance of the transaction to be resolved.
   * @param get_transaction_status_context The get transaction status context of
   * the remote transaction status operation.
   */
  virtual void OnRemoteTransactionNotFound(
      std::shared_ptr<Transaction>& transaction,
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept;

  /**
   * @brief The callback from the journal service to provide restored logs.
   *
   * @param bytes_buffer The bytes buffer containing the serialized logs.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult OnJournalServiceRecoverCallback(
      const std::shared_ptr<core::BytesBuffer>& bytes_buffer,
      const common::Uuid& activity_id) noexcept;

  /**
   * @brief Initializes transaction with the provided transaction context
   * metadata.
   *
   * @param transaction_context The transaction context containing transaction
   * metadata.
   * @param transaction The constructed transaction.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult InitializeTransaction(
      AsyncContext<TransactionRequest, TransactionResponse>&
          transaction_context,
      std::shared_ptr<Transaction>& transaction);

  /**
   * @brief Serializes the provided transaction and writes the output in the
   * output_buffer.
   *
   * @param transaction The transaction to be serialized.
   * @param output_buffer The output buffer to write the serialized transaction
   * to.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult SerializeTransaction(
      std::shared_ptr<Transaction>& transaction,
      BytesBuffer& output_buffer) noexcept;

  /**
   * @brief Serializes the provided transaction state and writes the output in
   * the output_buffer.
   *
   * @param transaction The transaction state to be serialized.
   * @param output_buffer The output buffer to write the serialized transaction
   * state to.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult SerializeState(
      std::shared_ptr<Transaction>& transaction,
      BytesBuffer& output_buffer) noexcept;

  /**
   * @brief Logs the transaction object and then proceeds to the next phase of
   * the transaction. This is always called before the transaction starts to
   * journal the transaction state.
   *
   * @param current_phase The current phase of the transaction.
   * @param transaction The transaction object.
   * @return ExecutionResult
   */
  virtual ExecutionResult LogTransactionAndProceedToNextPhase(
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Is called once the journaling operation is completed.
   *
   * @param journal_log_context The context of log operation.
   * @param current_phase The current phase of the transaction.
   * @param transaction The transaction object.
   */
  void OnLogTransactionCallback(
      AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context,
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Logs the current transaction state.
   *
   * @param current_phase The current phase of the transaction.
   * @param transaction The current transaction.
   * @param callback The callback to be called after logging the state.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult LogState(
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction,
      std::function<void(AsyncContext<JournalLogRequest, JournalLogResponse>&)>
          callback) noexcept;
  /**
   * @brief Logs the transaction's current state and then proceeds to the next
   * phase of the transaction. This is always called during the transaction
   * execution to store the state.
   *
   * @param current_phase The current phase of the transaction.
   * @param transaction The transaction object.
   * @return ExecutionResult
   */
  virtual ExecutionResult LogStateAndProceedToNextPhase(
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Logs the transaction's current state and then executes the
   * associated distributed commands.
   *
   * @param current_phase The current phase of the transaction.
   * @param transaction The transaction object.
   * @return ExecutionResult
   */
  virtual ExecutionResult LogStateAndExecuteDistributedPhase(
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Is called once the journaling operation is completed and the state
   * machine is about to proceed.
   *
   * @param journal_log_context The context of log operation.
   * @param current_phase The current phase of the transaction.
   * @param transaction The transaction object.
   */
  virtual void OnLogStateAndProceedToNextPhaseCallback(
      AsyncContext<JournalLogRequest, JournalLogResponse>& journal_log_context,
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Is called once the journaling operation is completed and the
   * distributed commands are about to be dispatched.
   *
   * @param journal_log_context The context of log operation.
   * @param current_phase The current phase of the transaction.
   * @param transaction The transaction object.
   */
  virtual void OnLogStateAndExecuteDistributedPhaseCallback(
      AsyncContext<JournalLogRequest, JournalLogResponse>& journal_log_context,
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Proceeds to the next phase of execution on the current transaction.
   *
   * @param current_phase The current phase of the transaction.
   * @param transaction The transaction object.
   */
  virtual void ProceedToNextPhase(
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Executes the current phase of the transaction.
   *
   * @param transaction The transaction object.
   */
  virtual void ExecuteCurrentPhase(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Proceeds to the next phase of execution on the current transaction
   * after recovery.
   *
   * @param transaction The transaction object.
   */
  virtual void ProceedToNextPhaseAfterRecovery(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Executes the Begin phase of a transaction.
   *
   * @param transaction The transaction object.
   */
  virtual void BeginTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Executes the Prepare phase of a transaction.
   *
   * @param transaction The transaction object.
   */
  virtual void PrepareTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Executes the Commit phase of a transaction.
   *
   * @param transaction The transaction object.
   */
  virtual void CommitTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Executes the CommitNotify phase of a transaction.
   *
   * @param transaction The transaction object.
   */
  virtual void CommitNotifyTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Executes the Committed phase of a transaction.
   *
   * @param transaction The transaction object.
   */
  virtual void CommittedTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Executes the AbortNotify phase of a transaction.
   *
   * @param transaction The transaction object.
   */
  virtual void AbortNotifyTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Executes the Aborted phase of a transaction.
   *
   * @param transaction The transaction object.
   */
  virtual void AbortedTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Executes the End phase of a transaction.
   *
   * @param transaction The transaction object.
   */
  virtual void EndTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief When a transaction gets into an unknown state.
   *
   * @param transaction The transaction object.
   */
  virtual void UnknownStateTransaction(
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Executes commands based on the transaction execution phase. This
   * only applies to the phases of the execution where there can be an
   * associated actions.
   *
   * @param current_phase The current phase of the transaction.
   * @param transaction The transaction object.
   */
  virtual void ExecuteDistributedPhase(
      transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Dispatches the command corresponding to the current phase of the
   * transaction.
   *
   * @param command_index The current command index.
   * @param current_phase The current phase of the transaction.
   * @param command The command to be executed.
   * @param transaction The transaction object.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult DispatchDistributedCommand(
      size_t command_index, transaction_manager::TransactionPhase current_phase,
      std::shared_ptr<TransactionCommand>& command,
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief At the end of any dispatch operation, it is required to wait for the
   * callbacks and go to the next phase as soon as all the callbacks complete.
   *
   * @param command_index The current command index.
   * @param current_phase The current phase of execution.
   * @param execution_result The execution of the callback.
   * @param transaction The transaction object.
   */
  virtual void OnPhaseCallback(
      size_t command_index, transaction_manager::TransactionPhase current_phase,
      ExecutionResult& execution_result,
      std::shared_ptr<Transaction>& transaction) noexcept;

  /**
   * @brief Converts transaction phase into proto transaction phase.
   *
   * @param phase The transaction phase.
   * @return transaction_manager::proto::TransactionPhase The proto transaction
   * phase.
   */
  virtual transaction_manager::proto::TransactionPhase ConvertPhaseToProtoPhase(
      transaction_manager::TransactionPhase phase) noexcept;

  /**
   * @brief Converts proto transaction phase into transaction phase.
   *
   * @param phase The proto transaction phase.
   * @return transaction_manager::TransactionPhase The transaction phase.
   */
  virtual transaction_manager::TransactionPhase ConvertProtoPhaseToPhase(
      transaction_manager::proto::TransactionPhase phase) noexcept;

  /**
   * @brief Converts the transaction phase to the transaction execution phase
   * enumerator.
   *
   * @param phase The current transaction phase.
   * @return TransactionExecutionPhase The converted transaction phase to the
   * transaction execution phase.
   */
  virtual TransactionExecutionPhase ToTransactionExecutionPhase(
      transaction_manager::TransactionPhase phase) noexcept;

  /**
   * @brief Convert TransactionPhase to String.
   *
   * @param phase
   * @return std::string
   */
  static std::string TransactionPhaseToString(
      transaction_manager::TransactionPhase);

  /**
   * @brief Convert TransactionExecutionPhase to String.
   *
   * @param phase
   * @return std::string
   */
  static std::string TransactionExecutionPhaseToString(
      TransactionExecutionPhase phase);

  /**
   * @brief Converts the transaction execution phase to the transaction
   * phase enumerator.
   *
   * @param phase The current transaction execution phase.
   * @return transaction_manager::TransactionPhase The converted transaction
   * execution phase to the transaction phase.
   */
  virtual transaction_manager::TransactionPhase ToTransactionPhase(
      TransactionExecutionPhase phase) noexcept;

  /// Indicates whether a transaction phase can be cancelled at the current
  /// state.
  virtual bool CanCancel(
      transaction_manager::TransactionPhase transaction_phase) noexcept;

  /**
   * @brief To execute a transaction phase, the caller must provide the
   * transaction and the desired phase to be executed.
   *
   * @param transaction The transaction to be executed.
   * @param transaction_phase_context The transaction phase context.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ExecutePhaseInternal(
      std::shared_ptr<Transaction>& transaction,
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
          transaction_phase_context) noexcept;

  /// The number of active transactions.
  std::atomic<size_t> active_transactions_count_;

  /**
   * @brief Active transactions map which maps transaction id to the transaction
   * object.
   *
   * NOTE: The map's garbage collection callback function
   * OnBeforeGarbageCollection is used a mechanism to resolve any pending
   * unresolved transactions that we have in the system.
   */
  common::AutoExpiryConcurrentMap<common::Uuid, std::shared_ptr<Transaction>,
                                  common::UuidCompare>
      active_transactions_map_;

  /// Provides statemachine functionality for each transaction.
  std::shared_ptr<transaction_manager::TransactionPhaseManagerInterface>
      transaction_phase_manager_;

  /// The remote transaction manager to run coordinated transactions.
  std::shared_ptr<RemoteTransactionManagerInterface>
      remote_transaction_manager_;

  /// An instance to the journal service.
  std::shared_ptr<JournalServiceInterface> journal_service_;

  /// An instance to the async executor.
  std::shared_ptr<AsyncExecutorInterface> async_executor_;

  /// An instance to transaction command serializer.
  std::shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer_;

  /// Operation dispatcher
  core::common::OperationDispatcher operation_dispatcher_;

  /// Metric client instance for custom metric recording.
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;

  /// Cache life time of the transactions
  size_t transaction_engine_cache_lifetime_seconds_;

  /// Config provider
  std::shared_ptr<ConfigProviderInterface> config_provider_;

  /// Indicates whether to skip logs that may cause log recovery to fail in
  /// this component.
  bool skip_log_recovery_failures_;

  /// Transaction lifetime in seconds
  size_t transaction_timeout_in_seconds_;

  /// Is resolution with remote coordinator enabled?
  bool transaction_resolution_with_remote_enabled_;

  /// Activity ID of background activities
  core::common::Uuid activity_id_;
};
}  // namespace google::scp::core
