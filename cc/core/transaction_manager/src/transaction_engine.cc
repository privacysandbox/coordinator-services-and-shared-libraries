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

#include "transaction_engine.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/common/serialization/src/serialization.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/http2_client/src/error_codes.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "core/transaction_manager/src/proto/transaction_engine.pb.h"

#include "error_codes.h"
#include "transaction_phase_manager.h"

using google::scp::core::JournalServiceInterface;
using google::scp::core::Version;
using google::scp::core::common::Serialization;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::Uuid;
using google::scp::core::transaction_manager::TransactionPhase;
using google::scp::core::transaction_manager::proto::TransactionCommandLog_1_0;
using google::scp::core::transaction_manager::proto::TransactionEngineLog;
using google::scp::core::transaction_manager::proto::TransactionEngineLog_1_0;
using google::scp::core::transaction_manager::proto::TransactionLog_1_0;
using google::scp::core::transaction_manager::proto::TransactionLogType;
using google::scp::core::transaction_manager::proto::TransactionPhaseLog_1_0;
using google::scp::cpio::MetricClientInterface;
using std::atomic;
using std::bind;
using std::function;
using std::list;
using std::make_pair;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::placeholders::_1;
using std::placeholders::_2;
using std::this_thread::sleep_for;

static constexpr size_t kStartupWaitIntervalMilliseconds = 1000;
static constexpr size_t kStopTransactionWaitLoopIntervalMilliseconds = 1000;
static constexpr Version kCurrentVersion = {.major = 1, .minor = 0};
// This value MUST NOT change forever.
static Uuid kTransactionEngineId = {.high = 0xFFFFFFF1, .low = 0x00000004};
static constexpr char kTransactionEngine[] = "TransactionEngine";

// Transaction Phase Log Strings
static constexpr char kTransactionPhaseNotStartedStr[] = "NOT_STARTED";
static constexpr char kTransactionPhaseBeginStr[] = "BEGIN";
static constexpr char kTransactionPhasePrepareStr[] = "PREPARE";
static constexpr char kTransactionPhaseCommitStr[] = "COMMIT";
static constexpr char kTransactionPhaseCommitNotifyStr[] = "COMMIT_NOTIFY";
static constexpr char kTransactionPhaseCommittedStr[] = "COMMITTED";
static constexpr char kTransactionPhaseAbortNotifyStr[] = "ABORT_NOTIFY";
static constexpr char kTransactionPhaseAbortedStr[] = "ABORTED";
static constexpr char kTransactionPhaseEndStr[] = "END";
static constexpr char kTransactionPhaseUnknownStr[] = "UNKNOWN";

#define INFO_CONTEXT_WITH_TRANSACTION_SCP_INFO(context, transaction, message, \
                                               ...)                           \
  {                                                                           \
    std::shared_ptr<Transaction>& __transaction = transaction;                \
    std::string __transaction_id_string =                                     \
        google::scp::core::common::ToString(__transaction->id);               \
    std::string __transaction_phase =                                         \
        TransactionPhaseToString(__transaction->current_phase.load());        \
    SCP_INFO_CONTEXT(kTransactionEngine, context,                             \
                     "Txn ID: %s, Phase: %s, " message,                       \
                     __transaction_id_string.c_str(),                         \
                     __transaction_phase.c_str(), ##__VA_ARGS__);             \
  }

#define DEBUG_CONTEXT_WITH_TRANSACTION_SCP_INFO(context, transaction, message, \
                                                ...)                           \
  {                                                                            \
    std::shared_ptr<Transaction>& __transaction = transaction;                 \
    std::string __transaction_id_string =                                      \
        google::scp::core::common::ToString(__transaction->id);                \
    std::string __transaction_phase =                                          \
        TransactionPhaseToString(__transaction->current_phase.load());         \
    size_t __transaction_last_execution_timestamp =                            \
        __transaction->last_execution_timestamp;                               \
    SCP_DEBUG_CONTEXT(                                                         \
        kTransactionEngine, context,                                           \
        "Txn ID: %s, Phase: %s, Last Execution TS: %llu, " message,            \
        __transaction_id_string.c_str(), __transaction_phase.c_str(),          \
        __transaction_last_execution_timestamp, ##__VA_ARGS__);                \
  }

#define ALERT_CONTEXT_WITH_TRANSACTION_SCP_INFO(                       \
    context, transaction, execution_result, message, ...)              \
  {                                                                    \
    std::shared_ptr<Transaction>& __transaction = transaction;         \
    std::string __transaction_id_string =                              \
        google::scp::core::common::ToString(__transaction->id);        \
    std::string __transaction_phase =                                  \
        TransactionPhaseToString(__transaction->current_phase.load()); \
    SCP_ALERT_CONTEXT(kTransactionEngine, context, execution_result,   \
                      "Txn ID: %s, Phase: %s, " message,               \
                      __transaction_id_string.c_str(),                 \
                      __transaction_phase.c_str(), ##__VA_ARGS__);     \
  }

#define ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(                       \
    context, transaction, execution_result, message, ...)              \
  {                                                                    \
    std::shared_ptr<Transaction>& __transaction = transaction;         \
    std::string __transaction_id_string =                              \
        google::scp::core::common::ToString(__transaction->id);        \
    std::string __transaction_phase =                                  \
        TransactionPhaseToString(__transaction->current_phase.load()); \
    SCP_ERROR_CONTEXT(kTransactionEngine, context, execution_result,   \
                      "Txn ID: %s, Phase: %s, " message,               \
                      __transaction_id_string.c_str(),                 \
                      __transaction_phase.c_str(), ##__VA_ARGS__);     \
  }

namespace google::scp::core {
TransactionEngine::TransactionEngine(
    shared_ptr<AsyncExecutorInterface>& async_executor,
    shared_ptr<TransactionCommandSerializerInterface>&
        transaction_command_serializer,
    shared_ptr<JournalServiceInterface>& journal_service,
    shared_ptr<RemoteTransactionManagerInterface>& remote_transaction_manager,
    const shared_ptr<MetricClientInterface>& metric_client,
    shared_ptr<ConfigProviderInterface> config_provider)
    : TransactionEngine(async_executor, transaction_command_serializer,
                        journal_service, make_shared<TransactionPhaseManager>(),
                        remote_transaction_manager, metric_client,
                        config_provider) {}

ExecutionResult TransactionEngine::Init() noexcept {
  auto execution_result = active_transactions_map_.Init();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  execution_result = config_provider_->Get(
      kTransactionManagerSkipFailedLogsInRecovery, skip_log_recovery_failures_);
  if (!execution_result.Successful()) {
    skip_log_recovery_failures_ = false;
  }

  execution_result = config_provider_->Get(
      kTransactionTimeoutInSecondsConfigName, transaction_timeout_in_seconds_);
  if (execution_result != SuccessExecutionResult()) {
    // Config not present, continue with default seconds
    transaction_timeout_in_seconds_ = kTransactionTimeoutSeconds;
  }

  execution_result =
      config_provider_->Get(kTransactionResolutionWithRemoteEnabled,
                            transaction_resolution_with_remote_enabled_);
  if (execution_result != SuccessExecutionResult()) {
    // Config not present, continue with default
    transaction_resolution_with_remote_enabled_ = true;
  }

  SCP_INFO(
      kTransactionEngine, activity_id_,
      "Initializing Transaction Engine.. Configured Transaction Timeout is "
      "%lld seconds, Transaction cache entry "
      "lifetime is %lld seconds, Skipping log recovery failures: %d",
      transaction_timeout_in_seconds_,
      transaction_engine_cache_lifetime_seconds_, skip_log_recovery_failures_)

  return journal_service_->SubscribeForRecovery(
      kTransactionEngineId,
      bind(&TransactionEngine::OnJournalServiceRecoverCallback, this, _1, _2));
}

ExecutionResult TransactionEngine::Run() noexcept {
  // All the pending transactions must be kicked off during the run operation.
  vector<Uuid> active_transaction_ids;
  auto execution_result = active_transactions_map_.Keys(active_transaction_ids);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  SCP_INFO(kTransactionEngine, activity_id_,
           "Transaction Engine has '%llu' active transactions to be resolved.",
           active_transactions_map_.Size());

  atomic<size_t> pending_calls(0);

  for (auto active_transaction_id : active_transaction_ids) {
    shared_ptr<Transaction> transaction;
    execution_result =
        active_transactions_map_.Find(active_transaction_id, transaction);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    // No need to recovery if the transaction is completed.
    if (transaction->current_phase == TransactionPhase::End) {
      continue;
    }

    pending_calls++;
    if (transaction->is_coordinated_remotely) {
      auto original_callback = transaction->remote_phase_context.callback;
      if (!original_callback) {
        original_callback = [](auto&) {};
      }

      transaction->remote_phase_context.callback =
          [original_callback, &pending_calls](
              AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                  context) {
            original_callback(context);
            pending_calls--;
          };
    }

    auto execution_result = async_executor_->Schedule(
        [this, transaction, &pending_calls]() mutable {
          ProceedToNextPhaseAfterRecovery(transaction);
          if (!transaction->is_coordinated_remotely) {
            pending_calls--;
          }

          return SuccessExecutionResult();
        },
        AsyncPriority::High);

    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  while (pending_calls.load() != 0) {
    SCP_INFO(kTransactionEngine, activity_id_,
             "Transaction Engine has '%llu' pending transactions to be "
             "resolved. Waiting...",
             pending_calls.load());
    sleep_for(milliseconds(kStartupWaitIntervalMilliseconds));
  }

  SCP_INFO(kTransactionEngine, activity_id_,
           "Transaction Engine finished resolving pending transactions");

  return active_transactions_map_.Run();
}

ExecutionResult TransactionEngine::Stop() noexcept {
  SCP_INFO(kTransactionEngine, activity_id_, "Transaction Engine stopping...");

  auto execution_result = active_transactions_map_.Stop();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  // Wait for pending activities to finish on the transactions
  vector<Uuid> active_transaction_ids;
  execution_result = active_transactions_map_.Keys(active_transaction_ids);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  SCP_INFO(kTransactionEngine, activity_id_,
           "Transaction Engine checking '%llu' active transactions if there is "
           "any pending work",
           active_transaction_ids.size());

  for (const auto& active_transaction_id : active_transaction_ids) {
    shared_ptr<Transaction> transaction;
    execution_result =
        active_transactions_map_.Find(active_transaction_id, transaction);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    // If transaction is not waiting for remote, it might be doing some
    // activity, so wait until it finishes.
    while (transaction->is_coordinated_remotely &&
           !transaction->is_waiting_for_remote) {
      SCP_INFO(kTransactionEngine, activity_id_,
               "Transaction Engine waiting on an active transaction '%s' that "
               "is busy doing some work",
               ToString(transaction->id).c_str());
      sleep_for(milliseconds(kStopTransactionWaitLoopIntervalMilliseconds));
    }
  }

  return execution_result;
}

TransactionEngine::~TransactionEngine() {
  if (journal_service_) {
    // Ignore the failure.
    journal_service_->UnsubscribeForRecovery(kTransactionEngineId);
  }
}

ExecutionResult TransactionEngine::LockRemotelyCoordinatedTransaction(
    shared_ptr<Transaction>& transaction) noexcept {
  if (!transaction->is_coordinated_remotely) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_COORDINATED_REMOTELY);
  }

  auto is_waiting_for_remote = true;
  if (!transaction->is_waiting_for_remote.compare_exchange_strong(
          is_waiting_for_remote, false)) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_CANNOT_BE_LOCKED);
  }
  return SuccessExecutionResult();
}

ExecutionResult TransactionEngine::UnlockRemotelyCoordinatedTransaction(
    shared_ptr<Transaction>& transaction) noexcept {
  if (!transaction->is_coordinated_remotely) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_COORDINATED_REMOTELY);
  }

  auto is_waiting_for_remote = false;
  if (!transaction->is_waiting_for_remote.compare_exchange_strong(
          is_waiting_for_remote, true)) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_CANNOT_BE_UNLOCKED);
  }

  return SuccessExecutionResult();
}

void TransactionEngine::OnBeforeGarbageCollection(
    Uuid& transaction_id, shared_ptr<Transaction>& transaction,
    function<void(bool)> should_delete_entry) noexcept {
  // Transaction entry is never deleted from the cache by the garbage
  // collection function of the map. The TransactionEngine itself does the
  // deletion explicitly when the transaction moves to a termination phase.
  should_delete_entry(false);
  ResolveTransaction(transaction);
}

ExecutionResult TransactionEngine::ResolveTransaction(
    shared_ptr<Transaction>& transaction) noexcept {
  if (!transaction->IsExpired() || transaction->blocked ||
      !transaction_resolution_with_remote_enabled_) {
    return SuccessExecutionResult();
  }

  auto transaction_secret = transaction->transaction_secret
                                ? *transaction->transaction_secret
                                : string();

  INFO_CONTEXT_WITH_TRANSACTION_SCP_INFO(
      transaction->context, transaction,
      "The transaction is expired and being auto-resolved. Secret: %s",
      transaction_secret.c_str());

  if (!transaction->is_coordinated_remotely) {
    // When rolling forward locally coordinated transactions, we try to cancel
    // the transaction if possible.
    if (CanCancel(transaction->current_phase)) {
      transaction->current_phase_execution_result = FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_TIMEOUT);
      transaction->current_phase_failed = true;
      ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
          transaction->context, transaction,
          transaction->current_phase_execution_result,
          "Locally coordinated transaction cancelled after timeout");
    }
    ProceedToNextPhase(transaction->current_phase, transaction);
    return SuccessExecutionResult();
  }

  auto pending_callbacks = transaction->pending_callbacks.load();
  if (pending_callbacks > 0) {
    ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        transaction->context, transaction,
        FailureExecutionResult(
            errors::SC_TRANSACTION_MANAGER_TRANSACTION_HAS_PENDING_CALLBACKS),
        "Cannot resolve when there are %zu pending callbacks.",
        pending_callbacks);
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_HAS_PENDING_CALLBACKS);
  }

  auto execution_result = LockRemotelyCoordinatedTransaction(transaction);
  if (!execution_result.Successful()) {
    ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(transaction->context, transaction,
                                            execution_result,
                                            "Cannot lock the transaction.");
    return execution_result;
  }

  INFO_CONTEXT_WITH_TRANSACTION_SCP_INFO(transaction->context, transaction,
                                         "Dispatching get transaction status.");

  // Get status of the other transaction being executed.
  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context(
          make_shared<GetTransactionStatusRequest>(),
          bind(&TransactionEngine::OnGetRemoteTransactionStatusCallback, this,
               transaction, _1),
          transaction->context);

  get_transaction_status_context.request->transaction_id = transaction->id;
  get_transaction_status_context.request->transaction_secret =
      transaction->transaction_secret;
  get_transaction_status_context.request->transaction_origin =
      transaction->transaction_origin;

  operation_dispatcher_.Dispatch<
      AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>>(
      get_transaction_status_context,
      [remote_transaction_manager = remote_transaction_manager_](
          AsyncContext<GetTransactionStatusRequest,
                       GetTransactionStatusResponse>&
              get_transaction_status_context) {
        return remote_transaction_manager->GetTransactionStatus(
            get_transaction_status_context);
      });

  return SuccessExecutionResult();
}

void TransactionEngine::OnGetRemoteTransactionStatusCallback(
    shared_ptr<Transaction>& transaction,
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
        get_transaction_status_context) noexcept {
  if (!get_transaction_status_context.result.Successful()) {
    if (get_transaction_status_context.result ==
            FailureExecutionResult(
                errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND) ||
        get_transaction_status_context.result ==
            FailureExecutionResult(
                errors::SC_HTTP2_CLIENT_HTTP_STATUS_NOT_FOUND)) {
      DEBUG_CONTEXT_WITH_TRANSACTION_SCP_INFO(
          get_transaction_status_context, transaction,
          "The transaction was not found on remote coordinator");

      OnRemoteTransactionNotFound(transaction, get_transaction_status_context);
      return;
    }

    ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        get_transaction_status_context, transaction,
        get_transaction_status_context.result,
        "Error finding transaction on remote coordinator.");

    // It has failed, unblock for another round of garbage collection.
    UnlockRemotelyCoordinatedTransaction(transaction);
    return;
  }

  if (!get_transaction_status_context.response->is_expired) {
    INFO_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        get_transaction_status_context, transaction,
        "The transaction is found on remote and is not expired yet.");
    UnlockRemotelyCoordinatedTransaction(transaction);
    return;
  }

  auto remote_transaction_phase = ToTransactionPhase(
      get_transaction_status_context.response->transaction_execution_phase);
  bool remote_transaction_failed =
      get_transaction_status_context.response->has_failure;

  // Check transactions to be in sync.
  if (!LocalAndRemoteTransactionsInSync(transaction->current_phase,
                                        remote_transaction_phase)) {
    ALERT_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        get_transaction_status_context, transaction,
        FailureExecutionResult(
            errors::SC_TRANSACTION_MANAGER_TRANSACTION_OUT_OF_SYNC),
        "The local and remote transactions are out of sync!, remote phase: %s. "
        "Blocking the transaction!",
        TransactionPhaseToString(remote_transaction_phase).c_str());
    transaction->blocked = true;
    UnlockRemotelyCoordinatedTransaction(transaction);
    return;
  }

  // Do nothing if the current phase is ahead of the other transaction.
  if (transaction->current_phase > remote_transaction_phase) {
    UnlockRemotelyCoordinatedTransaction(transaction);
    return;
  }

  // At this stage, if any of the transactions have failed, there should be
  // manual recovery.
  if (transaction->current_phase_failed) {
    ALERT_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        get_transaction_status_context, transaction,
        FailureExecutionResult(
            errors::SC_TRANSACTION_MANAGER_TRANSACTION_STUCK),
        "The transaction is stuck and cannot be automatically recovered!");
    UnlockRemotelyCoordinatedTransaction(transaction);
    return;
  }

  if (transaction->current_phase < remote_transaction_phase) {
    // Handle a case where the target is abort notify and the current is
    // commit
    RollForwardLocalTransaction(
        transaction,
        get_transaction_status_context.response->transaction_execution_phase,
        remote_transaction_failed);
    return;
  }

  if (transaction->current_phase == remote_transaction_phase) {
    RollForwardLocalAndRemoteTransactions(
        transaction,
        get_transaction_status_context.response->last_execution_timestamp);
    return;
  }
}

void TransactionEngine::OnRemoteTransactionNotFound(
    shared_ptr<Transaction>& transaction,
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
        get_transaction_status_context) noexcept {
  // If a transaction has just started Begin or Prepare phase on this TM
  // but not on the remote TM, and the client initiating the transaction has
  // crashed and stopped, then the corresponding transaction entry is not
  // found on the remote TM, so we need to clean it up here.
  // The same also applies when the transaction is in aborted, committed, or end
  // where the client has crashed and other TM has already removed the
  // transaction from its cache.
  if (transaction->current_phase != TransactionPhase::Begin &&
      transaction->current_phase != TransactionPhase::Prepare &&
      transaction->current_phase != TransactionPhase::Aborted &&
      transaction->current_phase != TransactionPhase::Committed &&
      transaction->current_phase != TransactionPhase::End) {
    ALERT_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        get_transaction_status_context, transaction,
        FailureExecutionResult(
            errors::SC_TRANSACTION_MANAGER_TRANSACTION_OUT_OF_SYNC),
        "The remote transaction was not found and the local "
        "transaction is out of sync. "
        "Transaction Expiration Timestamp: %zu, current transaction phase "
        "failed: %d. Blocking the transaction!",
        transaction->expiration_time, transaction->current_phase_failed.load());
    transaction->blocked = true;
    UnlockRemotelyCoordinatedTransaction(transaction);
    return;
  }

  // The transaction could be moved to END phase at this point.
  // Explicitly move the transaction to an End phase.
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context(
          make_shared<TransactionPhaseRequest>(),
          [](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&) {
          },
          get_transaction_status_context);

  transaction_phase_context.request->transaction_id = transaction->id;
  transaction_phase_context.request->transaction_secret =
      transaction->transaction_secret;
  transaction_phase_context.request->transaction_origin =
      transaction->transaction_origin;
  transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::End;
  transaction_phase_context.request->last_execution_timestamp =
      transaction->last_execution_timestamp;

  DEBUG_CONTEXT_WITH_TRANSACTION_SCP_INFO(
      get_transaction_status_context, transaction,
      "Automatically moving local transaction to END phase");

  // Proceed to the next phase. It is not required to wait for the
  // callback.
  auto execution_result =
      ExecutePhaseInternal(transaction, transaction_phase_context);
  if (!execution_result.Successful()) {
    UnlockRemotelyCoordinatedTransaction(transaction);
  }
}

ExecutionResult TransactionEngine::RollForwardLocalTransaction(
    shared_ptr<Transaction>& transaction, TransactionExecutionPhase next_phase,
    bool next_phase_has_failure) noexcept {
  INFO_CONTEXT_WITH_TRANSACTION_SCP_INFO(
      transaction->context, transaction,
      "Rolling forward the local transaction to the next phase %s",
      TransactionExecutionPhaseToString(next_phase).c_str());
  //  Handle a case where the target is abort notify and the current is
  //  commit
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context(
          make_shared<TransactionPhaseRequest>(),
          [](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&) {
          },
          transaction->context);
  transaction_phase_context.request->transaction_id = transaction->id;
  transaction_phase_context.request->transaction_secret =
      transaction->transaction_secret;
  transaction_phase_context.request->transaction_origin =
      transaction->transaction_origin;

  auto transaction_execution_phase =
      ToTransactionExecutionPhase(transaction->current_phase);

  if (transaction_execution_phase != TransactionExecutionPhase::End) {
    if ((next_phase == TransactionExecutionPhase::Abort) ||
        (next_phase == TransactionExecutionPhase::End &&
         next_phase_has_failure)) {
      transaction_execution_phase = TransactionExecutionPhase::Abort;
    }
  }

  transaction_phase_context.request->transaction_execution_phase =
      transaction_execution_phase;
  transaction_phase_context.request->last_execution_timestamp =
      transaction->last_execution_timestamp;
  auto execution_result =
      ExecutePhaseInternal(transaction, transaction_phase_context);
  if (!execution_result.Successful()) {
    UnlockRemotelyCoordinatedTransaction(transaction);
  }
  return execution_result;
}

ExecutionResult TransactionEngine::RollForwardLocalAndRemoteTransactions(
    shared_ptr<Transaction>& transaction,
    Timestamp last_execution_timestamp_remote) noexcept {
  INFO_CONTEXT_WITH_TRANSACTION_SCP_INFO(
      transaction->context, transaction,
      "Both local and remote transactions are on the same phase. Rolling "
      "forward the remote and the local transactions to the next "
      "phase");

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      remote_phase_context(
          make_shared<TransactionPhaseRequest>(),
          bind(&TransactionEngine::OnRollForwardRemoteTransactionCallback, this,
               transaction, _1),
          transaction->context);

  remote_phase_context.request->transaction_execution_phase =
      ToTransactionExecutionPhase(transaction->current_phase);

  if (CanCancel(transaction->current_phase)) {
    remote_phase_context.request->transaction_execution_phase =
        TransactionExecutionPhase::Abort;
    DEBUG_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        remote_phase_context, transaction,
        "Aborting Transaction. Moving remote transaction to phase: %s",
        TransactionExecutionPhaseToString(
            remote_phase_context.request->transaction_execution_phase)
            .c_str());
  }

  remote_phase_context.request->transaction_id = transaction->id;

  remote_phase_context.request->transaction_secret =
      transaction->transaction_secret;
  remote_phase_context.request->transaction_origin =
      transaction->transaction_origin;
  remote_phase_context.request->last_execution_timestamp =
      last_execution_timestamp_remote;

  // Proceed to the next phase. It is not required to wait for the callback.
  operation_dispatcher_.Dispatch<
      AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>>(
      remote_phase_context,
      [remote_transaction_manager = remote_transaction_manager_](
          AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
              remote_phase_context) {
        return remote_transaction_manager->ExecutePhase(remote_phase_context);
      });

  return SuccessExecutionResult();
}

void TransactionEngine::OnRollForwardRemoteTransactionCallback(
    shared_ptr<Transaction>& transaction,
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        remote_transaction_context) noexcept {
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      local_phase_context(
          make_shared<TransactionPhaseRequest>(),
          [](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&) {
          },
          transaction->context);

  if (!remote_transaction_context.result.Successful()) {
    ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        remote_transaction_context, transaction,
        remote_transaction_context.result,
        "Rolling forward the remote transaction failed.");

    UnlockRemotelyCoordinatedTransaction(transaction);
    return;
  }

  local_phase_context.request->transaction_execution_phase =
      remote_transaction_context.request->transaction_execution_phase;
  local_phase_context.request->transaction_id = transaction->id;
  local_phase_context.request->transaction_secret =
      transaction->transaction_secret;
  local_phase_context.request->transaction_origin =
      transaction->transaction_origin;
  local_phase_context.request->last_execution_timestamp =
      transaction->last_execution_timestamp;

  auto execution_result =
      ExecutePhaseInternal(transaction, local_phase_context);
  if (!execution_result.Successful()) {
    UnlockRemotelyCoordinatedTransaction(transaction);
  }
}

ExecutionResult TransactionEngine::OnJournalServiceRecoverCallback(
    const shared_ptr<BytesBuffer>& bytes_buffer,
    const Uuid& activity_id) noexcept {
  TransactionEngineLog transaction_engine_log;
  size_t offset = 0;
  size_t bytes_deserialized = 0;
  auto execution_result =
      Serialization::DeserializeProtoMessage<TransactionEngineLog>(
          *bytes_buffer, offset, bytes_buffer->length, transaction_engine_log,
          bytes_deserialized);
  if (!execution_result.Successful()) {
    SCP_ERROR(kTransactionEngine, activity_id, execution_result,
              "Log is corrupted, invalid TransactionEngineLog!");
    return execution_result;
  }

  execution_result =
      Serialization::ValidateVersion(transaction_engine_log, kCurrentVersion);
  if (!execution_result.Successful()) {
    SCP_ERROR(kTransactionEngine, activity_id, execution_result,
              "Log is corrupted, invalid TransactionEngineLog version!");
    return execution_result;
  }

  bytes_deserialized = 0;
  TransactionEngineLog_1_0 transaction_engine_log_1_0;
  execution_result =
      Serialization::DeserializeProtoMessage<TransactionEngineLog_1_0>(
          transaction_engine_log.log_body(), transaction_engine_log_1_0,
          bytes_deserialized);
  if (!execution_result.Successful()) {
    SCP_ERROR(kTransactionEngine, activity_id, execution_result,
              "Log is corrupted, invalid TransactionEngineLog_1_0!");
    return execution_result;
  }

  if (transaction_engine_log_1_0.type() ==
      TransactionLogType::TRANSACTION_LOG) {
    bytes_deserialized = 0;
    TransactionLog_1_0 transaction_log_1_0;
    execution_result =
        Serialization::DeserializeProtoMessage<TransactionLog_1_0>(
            transaction_engine_log_1_0.log_body(), transaction_log_1_0,
            bytes_deserialized);
    if (!execution_result.Successful()) {
      SCP_ERROR(kTransactionEngine, activity_id, execution_result,
                "Log is corrupted, invalid TransactionLog_1_0!");
      return execution_result;
    }

    // Construct the transaction id.
    Uuid transaction_id;
    transaction_id.high = transaction_log_1_0.id().high();
    transaction_id.low = transaction_log_1_0.id().low();

    auto transaction_id_string = common::ToString(transaction_id);
    SCP_DEBUG(
        kTransactionEngine, activity_id,
        "Recovering TRANSACTION_LOG for transaction %s, commands count: %zu",
        transaction_id_string.c_str(), transaction_log_1_0.commands_size());

    Timestamp timeout = transaction_log_1_0.timeout();
    bool is_coordinated_remotely =
        transaction_log_1_0.is_coordinated_remotely();
    auto transaction_secret =
        make_shared<string>(transaction_log_1_0.transaction_secret());
    auto transaction_origin =
        make_shared<string>(transaction_log_1_0.transaction_origin());
    AsyncContext<TransactionRequest, TransactionResponse> transaction_context(
        make_shared<TransactionRequest>(),
        [](AsyncContext<TransactionRequest, TransactionResponse>&) {},
        activity_id, activity_id);

    transaction_context.request->transaction_id = transaction_id;
    transaction_context.request->timeout_time = timeout;
    transaction_context.request->is_coordinated_remotely =
        is_coordinated_remotely;
    transaction_context.request->transaction_secret = transaction_secret;
    transaction_context.request->transaction_origin = transaction_origin;

    for (int command_index = 0;
         command_index < transaction_log_1_0.commands_size(); ++command_index) {
      auto command = transaction_log_1_0.commands(command_index);

      BytesBuffer command_bytes_buffer(command.command_body());

      shared_ptr<TransactionCommand> transaction_command = nullptr;
      auto execution_result = transaction_command_serializer_->Deserialize(
          transaction_id, command_bytes_buffer, transaction_command);
      if (!execution_result.Successful()) {
        return execution_result;
      }

      transaction_context.request->commands.push_back(transaction_command);
    }

    SCP_DEBUG(kTransactionEngine, activity_id,
              "Successfully processed TRANSACTION_LOG, recovered transaction "
              "with ID: %s",
              transaction_id_string.c_str());

    // The transaction might already be initialized due to a previous
    // duplicate log. InitializeTransaction is idempotent.
    shared_ptr<Transaction> transaction;
    return InitializeTransaction(transaction_context, transaction);
  }

  if (transaction_engine_log_1_0.type() ==
      TransactionLogType::TRANSACTION_PHASE_LOG) {
    bytes_deserialized = 0;
    TransactionPhaseLog_1_0 transaction_phase_log_1_0;
    execution_result =
        Serialization::DeserializeProtoMessage<TransactionPhaseLog_1_0>(
            transaction_engine_log_1_0.log_body(), transaction_phase_log_1_0,
            bytes_deserialized);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    Uuid transaction_id;
    transaction_id.high = transaction_phase_log_1_0.id().high();
    transaction_id.low = transaction_phase_log_1_0.id().low();

    auto target_transaction_phase =
        ConvertProtoPhaseToPhase(transaction_phase_log_1_0.phase());
    auto transaction_id_string = common::ToString(transaction_id);

    SCP_DEBUG(
        kTransactionEngine, activity_id,
        "Recovering TRANSACTION_PHASE_LOG with phase: %s of transaction %s",
        TransactionPhaseToString(target_transaction_phase).c_str(),
        transaction_id_string.c_str());

    shared_ptr<Transaction> transaction;
    auto execution_result =
        active_transactions_map_.Find(transaction_id, transaction);
    if (!execution_result.Successful()) {
      // End phase completes transaction recovery and removes transaction from
      // the active transactions map. If there is retry during End phase,
      // duplicate logs may occur, ignore them.
      if ((target_transaction_phase == TransactionPhase::End) ||
          skip_log_recovery_failures_) {
        return SuccessExecutionResult();
      }

      // This should never happen. Raise alert.
      SCP_ALERT(kTransactionEngine, activity_id, execution_result,
                "Cannot find the transaction with id %s for "
                "TRANSACTION_PHASE_LOG phase: %s",
                transaction_id_string.c_str(),
                TransactionPhaseToString(target_transaction_phase).c_str());

      return FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND);
    }

    if (target_transaction_phase < transaction->current_phase) {
      // This should never happen. Raise alert.
      SCP_ALERT(
          kTransactionEngine, activity_id, execution_result,
          "TRANSACTION_PHASE_LOG has stale phase %s. "
          "Current transaction phase is %s",
          TransactionPhaseToString(target_transaction_phase).c_str(),
          TransactionPhaseToString(transaction->current_phase.load()).c_str());
      if (skip_log_recovery_failures_) {
        return SuccessExecutionResult();
      }
      return FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_LOG);
    }

    transaction->current_phase = target_transaction_phase;
    transaction->transaction_failed = transaction_phase_log_1_0.failed();
    transaction->transaction_execution_result =
        ExecutionResult(transaction_phase_log_1_0.result());
    transaction->transaction_execution_result.status_code =
        transaction_phase_log_1_0.result().status_code();

    SCP_DEBUG(kTransactionEngine, activity_id,
              "Successfully processed TRANSACTION_PHASE_LOG, recovered "
              "transaction with ID: %s",
              transaction_id_string.c_str());

    // Remove the transaction if the end phase has been recovered.
    if (transaction->current_phase == TransactionPhase::End) {
      SCP_INFO(kTransactionEngine, activity_id,
               "Transaction recovery completed for transaction %s",
               transaction_id_string.c_str());
      return active_transactions_map_.Erase(transaction_id);
    }

    return SuccessExecutionResult();
  }

  return FailureExecutionResult(
      errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_LOG);
}

ExecutionResult TransactionEngine::InitializeTransaction(
    AsyncContext<TransactionRequest, TransactionResponse>& transaction_context,
    shared_ptr<Transaction>& transaction) {
  transaction = make_shared<Transaction>();
  transaction->id = transaction_context.request->transaction_id;
  transaction->context = transaction_context;
  transaction->current_phase = TransactionPhase::NotStarted;
  transaction->transaction_execution_result =
      FailureExecutionResult(errors::SC_TRANSACTION_MANAGER_NOT_FINISHED);
  transaction->is_coordinated_remotely =
      transaction_context.request->is_coordinated_remotely;
  transaction->transaction_secret =
      transaction_context.request->transaction_secret;
  transaction->transaction_origin =
      transaction_context.request->transaction_origin;
  if (transaction->is_coordinated_remotely) {
    if (!transaction->transaction_secret ||
        transaction->transaction_secret->length() == 0) {
      return FailureExecutionResult(
          errors::SC_TRANSACTION_ENGINE_INVALID_TRANSACTION_REQUEST);
    }
    if (!transaction->transaction_origin ||
        transaction->transaction_origin->length() == 0) {
      return FailureExecutionResult(
          errors::SC_TRANSACTION_ENGINE_INVALID_TRANSACTION_REQUEST);
    }
  }
  transaction->is_waiting_for_remote = false;
  transaction->current_phase_execution_result = SuccessExecutionResult();
  transaction->is_loaded = true;
  transaction->expiration_time =
      (TimeProvider::GetSteadyTimestampInNanoseconds() +
       seconds(transaction_timeout_in_seconds_))
          .count();
  transaction->last_execution_timestamp =
      TimeProvider::GetWallTimestampInNanosecondsAsClockTicks();
  auto pair = make_pair(transaction->id, transaction);
  auto execution_result = active_transactions_map_.Insert(pair, transaction);
  if (!execution_result.Successful()) {
    auto transaction_id_string = common::ToString(transaction->id);
    SCP_ERROR_CONTEXT(kTransactionEngine, transaction->context,
                      execution_result,
                      "Failed to insert the transaction id: %s",
                      transaction_id_string.c_str());
    if (execution_result.status_code ==
        core::errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS) {
      execution_result = FailureExecutionResult(
          core::errors::SC_TRANSACTION_MANAGER_TRANSACTION_ALREADY_EXISTS);
    }
  }
  return execution_result;
}

ExecutionResult TransactionEngine::SerializeTransaction(
    shared_ptr<Transaction>& transaction,
    BytesBuffer& transaction_engine_log_bytes_buffer) noexcept {
  TransactionEngineLog transaction_engine_log;
  transaction_engine_log.mutable_version()->set_major(kCurrentVersion.major);
  transaction_engine_log.mutable_version()->set_minor(kCurrentVersion.minor);

  TransactionEngineLog_1_0 transaction_engine_log_1_0;
  transaction_engine_log_1_0.set_type(TransactionLogType::TRANSACTION_LOG);

  TransactionLog_1_0 transaction_log_1_0;
  transaction_log_1_0.mutable_id()->set_high(transaction->id.high);
  transaction_log_1_0.mutable_id()->set_low(transaction->id.low);
  transaction_log_1_0.set_timeout(transaction->context.request->timeout_time);
  transaction_log_1_0.set_is_coordinated_remotely(
      transaction->context.request->is_coordinated_remotely);

  if (transaction->context.request->is_coordinated_remotely) {
    transaction_log_1_0.set_transaction_secret(
        *transaction->context.request->transaction_secret);
    transaction_log_1_0.set_transaction_origin(
        *transaction->context.request->transaction_origin);
  }

  for (size_t command_index = 0;
       command_index < transaction->context.request->commands.size();
       ++command_index) {
    auto current_command =
        transaction->context.request->commands[command_index];
    auto mutable_command = transaction_log_1_0.add_commands();

    BytesBuffer bytes_buffer;
    mutable_command->set_command_body(bytes_buffer.bytes->data(),
                                      bytes_buffer.length);
    auto execution_result = transaction_command_serializer_->Serialize(
        transaction->id, current_command, bytes_buffer);
    if (!execution_result.Successful()) {
      ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
          transaction->context, transaction, execution_result,
          "Cannot serialize the transaction commands.");
      return execution_result;
    }

    mutable_command->set_command_body(bytes_buffer.bytes->data(),
                                      bytes_buffer.length);
  }

  BytesBuffer transaction_log_1_0_bytes_buffer(
      transaction_log_1_0.ByteSizeLong());
  size_t offset = 0;
  size_t bytes_serialized = 0;
  auto execution_result =
      Serialization::SerializeProtoMessage<TransactionLog_1_0>(
          transaction_log_1_0_bytes_buffer, offset, transaction_log_1_0,
          bytes_serialized);
  if (!execution_result.Successful()) {
    ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        transaction->context, transaction, execution_result,
        "Cannot serialize the transaction log.");
    return execution_result;
  }
  transaction_log_1_0_bytes_buffer.length = bytes_serialized;

  transaction_engine_log_1_0.set_log_body(
      transaction_log_1_0_bytes_buffer.bytes->data(),
      transaction_log_1_0_bytes_buffer.length);

  bytes_serialized = 0;
  offset = 0;
  BytesBuffer transaction_engine_log_1_0_bytes_buffer(
      transaction_engine_log_1_0.ByteSizeLong());
  execution_result =
      Serialization::SerializeProtoMessage<TransactionEngineLog_1_0>(
          transaction_engine_log_1_0_bytes_buffer, offset,
          transaction_engine_log_1_0, bytes_serialized);
  if (!execution_result.Successful()) {
    ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        transaction->context, transaction, execution_result,
        "Cannot serialize the transaction engine log 1.0.");
    return execution_result;
  }
  transaction_engine_log_1_0_bytes_buffer.length = bytes_serialized;
  transaction_engine_log.set_log_body(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(), bytes_serialized);

  offset = 0;
  bytes_serialized = 0;
  transaction_engine_log_bytes_buffer.bytes =
      make_shared<vector<Byte>>(transaction_engine_log.ByteSizeLong());
  transaction_engine_log_bytes_buffer.capacity =
      transaction_engine_log.ByteSizeLong();
  execution_result = Serialization::SerializeProtoMessage<TransactionEngineLog>(
      transaction_engine_log_bytes_buffer, offset, transaction_engine_log,
      bytes_serialized);
  if (!execution_result.Successful()) {
    ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        transaction->context, transaction, execution_result,
        "Cannot serialize the transaction engine log.");
    return execution_result;
  }
  transaction_engine_log_bytes_buffer.length = bytes_serialized;
  return SuccessExecutionResult();
}

ExecutionResult TransactionEngine::LogTransactionAndProceedToNextPhase(
    TransactionPhase current_phase,
    shared_ptr<Transaction>& transaction) noexcept {
  BytesBuffer transaction_engine_log_bytes_buffer;
  auto execution_result =
      SerializeTransaction(transaction, transaction_engine_log_bytes_buffer);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.parent_activity_id = transaction->context.activity_id;
  journal_log_context.correlation_id = transaction->context.correlation_id;
  journal_log_context.request = make_shared<JournalLogRequest>();
  journal_log_context.request->component_id = kTransactionEngineId;
  journal_log_context.request->log_id = Uuid::GenerateUuid();
  journal_log_context.request->log_status = JournalLogStatus::Log;
  journal_log_context.request->data =
      make_shared<BytesBuffer>(transaction_engine_log_bytes_buffer);
  journal_log_context.callback =
      bind(&TransactionEngine::OnLogTransactionCallback, this, _1,
           current_phase, transaction);

  operation_dispatcher_
      .Dispatch<AsyncContext<JournalLogRequest, JournalLogResponse>>(
          journal_log_context,
          [journal_service = journal_service_](
              AsyncContext<JournalLogRequest, JournalLogResponse>&
                  journal_log_context) {
            return journal_service->Log(journal_log_context);
          });

  return SuccessExecutionResult();
}

void TransactionEngine::OnLogTransactionCallback(
    AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context,
    TransactionPhase current_phase,
    shared_ptr<Transaction>& transaction) noexcept {
  if (!journal_log_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kTransactionEngine, journal_log_context,
                      journal_log_context.result,
                      "On log transaction callback failed.");
    operation_dispatcher_
        .Dispatch<AsyncContext<JournalLogRequest, JournalLogResponse>>(
            journal_log_context,
            [journal_service = journal_service_](
                AsyncContext<JournalLogRequest, JournalLogResponse>&
                    journal_log_context) {
              return journal_service->Log(journal_log_context);
            });
    return;
  }

  ProceedToNextPhase(current_phase, transaction);
}

ExecutionResult TransactionEngine::SerializeState(
    std::shared_ptr<Transaction>& transaction,
    BytesBuffer& transaction_engine_log_bytes_buffer) noexcept {
  TransactionEngineLog transaction_engine_log;
  transaction_engine_log.mutable_version()->set_major(kCurrentVersion.major);
  transaction_engine_log.mutable_version()->set_minor(kCurrentVersion.minor);

  TransactionEngineLog_1_0 transaction_engine_log_1_0;
  transaction_engine_log_1_0.set_type(
      TransactionLogType::TRANSACTION_PHASE_LOG);

  TransactionPhaseLog_1_0 transaction_phase_log_1_0;
  transaction_phase_log_1_0.mutable_id()->set_high(transaction->id.high);
  transaction_phase_log_1_0.mutable_id()->set_low(transaction->id.low);
  transaction_phase_log_1_0.set_phase(
      ConvertPhaseToProtoPhase(transaction->current_phase.load()));
  transaction_phase_log_1_0.set_failed(transaction->transaction_failed.load());
  transaction_phase_log_1_0.mutable_result()->set_status(
      ToStatusProto(transaction->transaction_execution_result.status));
  transaction_phase_log_1_0.mutable_result()->set_status_code(
      transaction->transaction_execution_result.status_code);

  size_t offset = 0;
  size_t bytes_serialized = 0;
  BytesBuffer transaction_phase_log_1_0_bytes_buffer(
      transaction_phase_log_1_0.ByteSizeLong());

  auto execution_result =
      Serialization::SerializeProtoMessage<TransactionPhaseLog_1_0>(
          transaction_phase_log_1_0_bytes_buffer, offset,
          transaction_phase_log_1_0, bytes_serialized);
  if (!execution_result.Successful()) {
    ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        transaction->context, transaction, execution_result,
        "Cannot serialize the transaction phase.");
    return execution_result;
  }
  transaction_phase_log_1_0_bytes_buffer.length = bytes_serialized;

  transaction_engine_log_1_0.set_log_body(
      transaction_phase_log_1_0_bytes_buffer.bytes->data(),
      transaction_phase_log_1_0_bytes_buffer.length);

  offset = 0;
  bytes_serialized = 0;
  BytesBuffer transaction_engine_log_1_0_bytes_buffer(
      transaction_engine_log_1_0.ByteSizeLong());
  execution_result =
      Serialization::SerializeProtoMessage<TransactionEngineLog_1_0>(
          transaction_engine_log_1_0_bytes_buffer, offset,
          transaction_engine_log_1_0, bytes_serialized);
  if (!execution_result.Successful()) {
    ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        transaction->context, transaction, execution_result,
        "Cannot serialize the transaction engine log 1.0.");
    return execution_result;
  }
  transaction_engine_log_1_0_bytes_buffer.length = bytes_serialized;
  transaction_engine_log.set_log_body(
      transaction_engine_log_1_0_bytes_buffer.bytes->data(),
      transaction_engine_log_1_0_bytes_buffer.length);

  offset = 0;
  bytes_serialized = 0;
  transaction_engine_log_bytes_buffer.bytes =
      make_shared<vector<Byte>>(transaction_engine_log.ByteSizeLong());
  transaction_engine_log_bytes_buffer.capacity =
      transaction_engine_log.ByteSizeLong();
  execution_result = Serialization::SerializeProtoMessage<TransactionEngineLog>(
      transaction_engine_log_bytes_buffer, offset, transaction_engine_log,
      bytes_serialized);
  if (!execution_result.Successful()) {
    ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        transaction->context, transaction, execution_result,
        "Cannot serialize the transaction engine log.");
    return execution_result;
  }
  transaction_engine_log_bytes_buffer.length = bytes_serialized;
  return SuccessExecutionResult();
}

ExecutionResult TransactionEngine::LogState(
    TransactionPhase current_phase, shared_ptr<Transaction>& transaction,
    function<void(AsyncContext<JournalLogRequest, JournalLogResponse>&)>
        callback) noexcept {
  BytesBuffer transaction_engine_log_bytes_buffer;
  auto execution_result =
      SerializeState(transaction, transaction_engine_log_bytes_buffer);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  AsyncContext<JournalLogRequest, JournalLogResponse> journal_log_context;
  journal_log_context.parent_activity_id = transaction->context.activity_id;
  journal_log_context.correlation_id = transaction->context.correlation_id;
  journal_log_context.request = make_shared<JournalLogRequest>();
  journal_log_context.request->component_id = kTransactionEngineId;
  journal_log_context.request->log_id = Uuid::GenerateUuid();
  journal_log_context.request->log_status = JournalLogStatus::Log;
  journal_log_context.request->data =
      make_shared<BytesBuffer>(transaction_engine_log_bytes_buffer);
  journal_log_context.callback = callback;

  operation_dispatcher_
      .Dispatch<AsyncContext<JournalLogRequest, JournalLogResponse>>(
          journal_log_context,
          [journal_service = journal_service_](
              AsyncContext<JournalLogRequest, JournalLogResponse>&
                  journal_log_context) {
            return journal_service->Log(journal_log_context);
          });

  return SuccessExecutionResult();
}

ExecutionResult TransactionEngine::LogStateAndProceedToNextPhase(
    TransactionPhase current_phase,
    shared_ptr<Transaction>& transaction) noexcept {
  return LogState(
      current_phase, transaction,
      bind(&TransactionEngine::OnLogStateAndProceedToNextPhaseCallback, this,
           _1, current_phase, transaction));
}

ExecutionResult TransactionEngine::LogStateAndExecuteDistributedPhase(
    TransactionPhase current_phase,
    shared_ptr<Transaction>& transaction) noexcept {
  return LogState(
      current_phase, transaction,
      bind(&TransactionEngine::OnLogStateAndExecuteDistributedPhaseCallback,
           this, _1, current_phase, transaction));
}

void TransactionEngine::OnLogStateAndProceedToNextPhaseCallback(
    AsyncContext<JournalLogRequest, JournalLogResponse>& journal_log_context,
    TransactionPhase current_phase,
    shared_ptr<Transaction>& transaction) noexcept {
  if (!journal_log_context.result.Successful()) {
    ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(journal_log_context, transaction,
                                            journal_log_context.result,
                                            "Transaction State WAL failed.");
    operation_dispatcher_
        .Dispatch<AsyncContext<JournalLogRequest, JournalLogResponse>>(
            journal_log_context,
            [journal_service = journal_service_](
                AsyncContext<JournalLogRequest, JournalLogResponse>&
                    journal_log_context) {
              return journal_service->Log(journal_log_context);
            });
    return;
  }

  ProceedToNextPhase(current_phase, transaction);
}

void TransactionEngine::OnLogStateAndExecuteDistributedPhaseCallback(
    AsyncContext<JournalLogRequest, JournalLogResponse>& journal_log_context,
    TransactionPhase current_phase,
    shared_ptr<Transaction>& transaction) noexcept {
  if (!journal_log_context.result.Successful()) {
    ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(journal_log_context, transaction,
                                            journal_log_context.result,
                                            "Transaction State WAL failed.");
    operation_dispatcher_
        .Dispatch<AsyncContext<JournalLogRequest, JournalLogResponse>>(
            journal_log_context,
            [journal_service = journal_service_](
                AsyncContext<JournalLogRequest, JournalLogResponse>&
                    journal_log_context) {
              return journal_service->Log(journal_log_context);
            });
    return;
  }

  ExecuteDistributedPhase(current_phase, transaction);
}

ExecutionResult TransactionEngine::Execute(
    AsyncContext<TransactionRequest, TransactionResponse>&
        transaction_context) noexcept {
  shared_ptr<Transaction> transaction = nullptr;
  auto execution_result =
      InitializeTransaction(transaction_context, transaction);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  return LogTransactionAndProceedToNextPhase(TransactionPhase::NotStarted,
                                             transaction);
}

ExecutionResult TransactionEngine::ExecutePhase(
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        transaction_phase_context) noexcept {
  shared_ptr<Transaction> transaction;
  auto execution_result = active_transactions_map_.Find(
      transaction_phase_context.request->transaction_id, transaction);

  if (!execution_result.Successful()) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND);
  }

  if (!transaction->is_coordinated_remotely) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_COORDINATED_REMOTELY);
  }

  if (!transaction->transaction_secret ||
      transaction->transaction_secret->compare(
          *transaction_phase_context.request->transaction_secret) != 0) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_SECRET_IS_NOT_VALID);
  }

  if (!transaction->IsExpired()) {
    if (!transaction->transaction_origin ||
        transaction->transaction_origin->compare(
            *transaction_phase_context.request->transaction_origin) != 0) {
      return FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_ORIGIN_IS_NOT_VALID);
    }
  }

  execution_result = LockRemotelyCoordinatedTransaction(transaction);
  if (!execution_result.Successful()) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_CURRENT_TRANSACTION_IS_RUNNING);
  }

  execution_result =
      ExecutePhaseInternal(transaction, transaction_phase_context);
  if (!execution_result.Successful()) {
    UnlockRemotelyCoordinatedTransaction(transaction);
  }
  return execution_result;
}

ExecutionResult TransactionEngine::ExecutePhaseInternal(
    shared_ptr<Transaction>& transaction,
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        transaction_phase_context) noexcept {
  if (transaction->is_waiting_for_remote ||
      !transaction->is_coordinated_remotely) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_IS_NOT_LOCKED);
  }

  // There is a chance that a callback has removed this element from the map
  // before.
  auto execution_result =
      active_transactions_map_.Find(transaction->id, transaction);
  if (!execution_result.Successful()) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND);
  }

  if (transaction->last_execution_timestamp !=
      transaction_phase_context.request->last_execution_timestamp) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_LAST_EXECUTION_TIMESTAMP_NOT_MATCHING);
  }

  auto requested_phase =
      transaction_phase_context.request->transaction_execution_phase;
  if (requested_phase == TransactionExecutionPhase::Unknown ||
      transaction->current_phase == TransactionPhase::Unknown) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE);
  }

  if (requested_phase == TransactionExecutionPhase::Begin) {
    if (transaction->current_phase != TransactionPhase::Begin) {
      return FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE);
    }
  }

  if (requested_phase == TransactionExecutionPhase::Prepare) {
    if (transaction->current_phase != TransactionPhase::Prepare) {
      return FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE);
    }
  }

  if (requested_phase == TransactionExecutionPhase::Commit) {
    if (transaction->current_phase != TransactionPhase::Commit) {
      return FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE);
    }
  }

  if (requested_phase == TransactionExecutionPhase::Notify) {
    if (transaction->current_phase != TransactionPhase::CommitNotify) {
      return FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE);
    }
  }

  if (requested_phase == TransactionExecutionPhase::Abort) {
    if (!TransactionPhaseManager::CanProceedToAbortAtPhase(
            transaction->current_phase)) {
      return FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE);
    }

    transaction->current_phase = TransactionPhase::AbortNotify;
  }

  if (requested_phase == TransactionExecutionPhase::End) {
    if (!TransactionPhaseManager::CanProceedToEndAtPhase(
            transaction->current_phase)) {
      return FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE);
    }

    transaction->current_phase = TransactionPhase::End;
  }

  transaction->remote_phase_context = transaction_phase_context;

  switch (requested_phase) {
    case TransactionExecutionPhase::Begin:
      BeginTransaction(transaction);
      break;
    case TransactionExecutionPhase::Prepare:
      PrepareTransaction(transaction);
      break;
    case TransactionExecutionPhase::Commit:
      CommitTransaction(transaction);
      break;
    case TransactionExecutionPhase::Notify:
      CommitNotifyTransaction(transaction);
      break;
    case TransactionExecutionPhase::Abort:
      transaction->transaction_failed = true;
      transaction->transaction_execution_result = FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_TRANSACTION_IS_ABORTED);
      transaction->current_phase_failed = false;
      transaction->current_phase_execution_result = SuccessExecutionResult();
      AbortNotifyTransaction(transaction);
      break;
    case TransactionExecutionPhase::End:
      EndTransaction(transaction);
      break;
    case TransactionExecutionPhase::Unknown:
    default:
      return FailureExecutionResult(
          errors::SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE);
  }

  return SuccessExecutionResult();
}

ExecutionResult TransactionEngine::GetTransactionStatus(
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
        get_transaction_status_context) noexcept {
  shared_ptr<Transaction> transaction;
  auto execution_result = active_transactions_map_.Find(
      get_transaction_status_context.request->transaction_id, transaction);
  if (!execution_result.Successful()) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND);
  }

  if (!transaction->is_coordinated_remotely) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_COORDINATED_REMOTELY);
  }

  if (!get_transaction_status_context.request->transaction_secret ||
      transaction->transaction_secret->compare(
          *get_transaction_status_context.request->transaction_secret) != 0) {
    return FailureExecutionResult(
        errors::SC_TRANSACTION_MANAGER_TRANSACTION_SECRET_IS_NOT_VALID);
  }

  get_transaction_status_context.result = SuccessExecutionResult();
  get_transaction_status_context.response =
      make_shared<GetTransactionStatusResponse>();
  get_transaction_status_context.response->has_failure =
      transaction->transaction_failed || transaction->current_phase_failed;
  get_transaction_status_context.response->transaction_execution_phase =
      ToTransactionExecutionPhase(transaction->current_phase);
  get_transaction_status_context.response->last_execution_timestamp =
      transaction->last_execution_timestamp;
  get_transaction_status_context.response->is_expired =
      transaction->IsExpired();

  DEBUG_CONTEXT_WITH_TRANSACTION_SCP_INFO(
      get_transaction_status_context, transaction,
      "GetTransactionStatus Finished. IsExpired: %d, HasFailure: %d",
      get_transaction_status_context.response->is_expired,
      get_transaction_status_context.response->has_failure);

  get_transaction_status_context.Finish();
  return SuccessExecutionResult();
}

ExecutionResult TransactionEngine::Checkpoint(
    shared_ptr<list<CheckpointLog>>& checkpoint_logs) noexcept {
  vector<Uuid> active_transactions;
  auto execution_result = active_transactions_map_.Keys(active_transactions);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  SCP_DEBUG(kTransactionEngine, activity_id_,
            "Number of active transactions in map to checkpoint: %llu",
            active_transactions.size());
  for (auto& active_transaction : active_transactions) {
    // 1) Serialize the transaction.
    // 2) Serialize the current state.
    shared_ptr<Transaction> transaction;
    execution_result =
        active_transactions_map_.Find(active_transaction, transaction);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    CheckpointLog transaction_checkpoint_log;
    execution_result = SerializeTransaction(
        transaction, transaction_checkpoint_log.bytes_buffer);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    transaction_checkpoint_log.component_id = kTransactionEngineId;
    transaction_checkpoint_log.log_id = Uuid::GenerateUuid();
    transaction_checkpoint_log.log_status = JournalLogStatus::Log;

    CheckpointLog state_metadata;
    execution_result = SerializeState(transaction, state_metadata.bytes_buffer);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    state_metadata.component_id = kTransactionEngineId;
    state_metadata.log_id = Uuid::GenerateUuid();
    state_metadata.log_status = JournalLogStatus::Log;

    checkpoint_logs->push_back(move(transaction_checkpoint_log));
    checkpoint_logs->push_back(move(state_metadata));
  }

  return SuccessExecutionResult();
}

void TransactionEngine::ProceedToNextPhaseAfterRecovery(
    shared_ptr<Transaction>& transaction) noexcept {
  switch (transaction->current_phase) {
    case TransactionPhase::NotStarted:
    case TransactionPhase::Begin:
      transaction->current_phase = TransactionPhase::Begin;
      BeginTransaction(transaction);
      return;
    case TransactionPhase::Prepare:
      PrepareTransaction(transaction);
      return;
    case TransactionPhase::Commit:
      CommitTransaction(transaction);
      return;
    case TransactionPhase::CommitNotify:
      CommitNotifyTransaction(transaction);
      return;
    case TransactionPhase::Committed:
      CommittedTransaction(transaction);
      return;
    case TransactionPhase::AbortNotify:
      AbortNotifyTransaction(transaction);
      return;
    case TransactionPhase::Aborted:
      AbortedTransaction(transaction);
      return;
    case TransactionPhase::End:
      EndTransaction(transaction);
      return;
    default:
      UnknownStateTransaction(transaction);
  }
}

void TransactionEngine::ProceedToNextPhase(
    TransactionPhase current_phase,
    shared_ptr<Transaction>& transaction) noexcept {
  if (current_phase == TransactionPhase::End) {
    transaction->last_execution_timestamp =
        TimeProvider::GetWallTimestampInNanosecondsAsClockTicks();

    auto delete_transaction = false;
    if (transaction->is_coordinated_remotely) {
      transaction->remote_phase_context.result =
          transaction->current_phase_execution_result;
      transaction->remote_phase_context.response =
          make_shared<TransactionPhaseResponse>();
      transaction->remote_phase_context.response->last_execution_timestamp =
          transaction->last_execution_timestamp;
      transaction->remote_phase_context.Finish();

      if (transaction->remote_phase_context.result ==
          SuccessExecutionResult()) {
        delete_transaction = true;
      }
    } else {
      transaction->context.response = make_shared<TransactionResponse>();
      transaction->context.response->transaction_id = transaction->id;
      transaction->context.response->last_execution_timestamp =
          transaction->last_execution_timestamp;
      transaction->context.result =
          transaction->transaction_failed
              ? transaction->transaction_execution_result
              : SuccessExecutionResult();
      transaction->context.Finish();

      if (transaction->context.result.Successful()) {
        delete_transaction = true;
      }
    }

    if (delete_transaction) {
      active_transactions_map_.Erase(transaction->id);
    }
    return;
  }

  auto next_phase = transaction_phase_manager_->ProceedToNextPhase(
      current_phase, transaction->current_phase_execution_result);

  if (next_phase == TransactionPhase::Unknown) {
    ALERT_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        transaction->context, transaction,
        FailureExecutionResult(
            errors::SC_TRANSACTION_MANAGER_TRANSACTION_UNKNOWN),
        "Got next phase as UNKNOWN. The current state of the transaction is "
        "not valid.");
  }

  auto current_phase_execution_result =
      transaction->current_phase_execution_result;

  list<size_t> failed_indices;
  if (transaction->current_phase_failed) {
    auto failed = false;
    size_t command_index = 0;
    while (transaction->current_phase_failed_command_indices
               .TryDequeue(command_index)
               .Successful()) {
      failed_indices.push_back(command_index);
    }

    // Only change if the current status was false.
    if (transaction->transaction_failed.compare_exchange_strong(failed, true)) {
      transaction->transaction_execution_result =
          transaction->current_phase_execution_result;
    }
    // Reset the state before continuing to the next phase.
    transaction->current_phase_failed = false;
    transaction->current_phase_execution_result = SuccessExecutionResult();
  }

  // It is required to ensure that the state will be change from a phase to
  // another phase only once.
  if (!transaction->current_phase.compare_exchange_strong(current_phase,
                                                          next_phase)) {
    ALERT_CONTEXT_WITH_TRANSACTION_SCP_INFO(transaction->context, transaction,
                                            FailureExecutionResult(SC_UNKNOWN),
                                            "Dangling callback in TM");
    return;
  }

  transaction->last_execution_timestamp =
      TimeProvider::GetWallTimestampInNanosecondsAsClockTicks();

  if (transaction->is_coordinated_remotely) {
    // Filter commands corresponding to the failed indices for sending them
    // back to the caller
    list<shared_ptr<TransactionCommand>> failed_commands;
    for (const auto& failed_index : failed_indices) {
      failed_commands.push_back(
          transaction->context.request->commands[failed_index]);
    }
    if (transaction->current_phase == TransactionPhase::Begin) {
      transaction->context.response = make_shared<TransactionResponse>();
      transaction->context.response->transaction_id = transaction->id;
      transaction->context.response->last_execution_timestamp =
          transaction->last_execution_timestamp;
      transaction->context.response->failed_commands_indices =
          move(failed_indices);
      transaction->context.response->failed_commands = move(failed_commands);
      transaction->context.result = current_phase_execution_result;
      UnlockRemotelyCoordinatedTransaction(transaction);
      transaction->context.Finish();
    } else {
      transaction->remote_phase_context.result = current_phase_execution_result;
      transaction->remote_phase_context.response =
          make_shared<TransactionPhaseResponse>();
      transaction->remote_phase_context.response->last_execution_timestamp =
          transaction->last_execution_timestamp;
      transaction->remote_phase_context.response->failed_commands_indices =
          move(failed_indices);
      transaction->remote_phase_context.response->failed_commands =
          move(failed_commands);
      transaction->remote_phase_context.Finish();
      UnlockRemotelyCoordinatedTransaction(transaction);
    }
    return;
  }

  ExecuteCurrentPhase(transaction);
}

void TransactionEngine::ExecuteCurrentPhase(
    shared_ptr<Transaction>& transaction) noexcept {
  switch (transaction->current_phase) {
    case TransactionPhase::Begin:
      BeginTransaction(transaction);
      return;
    case TransactionPhase::Prepare:
      PrepareTransaction(transaction);
      return;
    case TransactionPhase::Commit:
      CommitTransaction(transaction);
      return;
    case TransactionPhase::CommitNotify:
      CommitNotifyTransaction(transaction);
      return;
    case TransactionPhase::Committed:
      CommittedTransaction(transaction);
      return;
    case TransactionPhase::AbortNotify:
      AbortNotifyTransaction(transaction);
      return;
    case TransactionPhase::Aborted:
      AbortedTransaction(transaction);
      return;
    case TransactionPhase::End:
      EndTransaction(transaction);
      return;
    default:
      UnknownStateTransaction(transaction);
  }
}

void TransactionEngine::BeginTransaction(
    shared_ptr<Transaction>& transaction) noexcept {
  ExecuteDistributedPhase(TransactionPhase::Begin, transaction);
}

void TransactionEngine::PrepareTransaction(
    shared_ptr<Transaction>& transaction) noexcept {
  LogStateAndExecuteDistributedPhase(TransactionPhase::Prepare, transaction);
}

void TransactionEngine::CommitTransaction(
    shared_ptr<Transaction>& transaction) noexcept {
  LogStateAndExecuteDistributedPhase(TransactionPhase::Commit, transaction);
}

void TransactionEngine::CommitNotifyTransaction(
    shared_ptr<Transaction>& transaction) noexcept {
  LogStateAndExecuteDistributedPhase(TransactionPhase::CommitNotify,
                                     transaction);
}

void TransactionEngine::CommittedTransaction(
    shared_ptr<Transaction>& transaction) noexcept {
  LogStateAndProceedToNextPhase(TransactionPhase::Committed, transaction);
}

void TransactionEngine::AbortNotifyTransaction(
    shared_ptr<Transaction>& transaction) noexcept {
  LogStateAndExecuteDistributedPhase(TransactionPhase::AbortNotify,
                                     transaction);
}

void TransactionEngine::AbortedTransaction(
    shared_ptr<Transaction>& transaction) noexcept {
  LogStateAndProceedToNextPhase(TransactionPhase::Aborted, transaction);
}

void TransactionEngine::EndTransaction(
    shared_ptr<Transaction>& transaction) noexcept {
  LogStateAndExecuteDistributedPhase(TransactionPhase::End, transaction);
}

void TransactionEngine::UnknownStateTransaction(
    shared_ptr<Transaction>& transaction) noexcept {
  ALERT_CONTEXT_WITH_TRANSACTION_SCP_INFO(
      transaction->context, transaction,
      FailureExecutionResult(errors::SC_TRANSACTION_MANAGER_STATE_IS_INVALID),
      "Transaction has reached unknown state");
}

void TransactionEngine::ExecuteDistributedPhase(
    TransactionPhase current_phase,
    shared_ptr<Transaction>& transaction) noexcept {
  transaction->pending_callbacks =
      transaction->context.request->commands.size();
  uint64_t failed_operations = 0;
  for (size_t i = 0; i < transaction->context.request->commands.size(); ++i) {
    auto command = transaction->context.request->commands.at(i);
    auto execution_result =
        DispatchDistributedCommand(i, current_phase, command, transaction);
    if (!execution_result.Successful()) {
      auto failed = false;

      if (execution_result.status == ExecutionStatus::Failure) {
        auto enqueue_execution_result =
            transaction->current_phase_failed_command_indices.TryEnqueue(i);
        if (!enqueue_execution_result.Successful()) {
          ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
              transaction->context, transaction,
              FailureExecutionResult(
                  errors::SC_TRANSACTION_MANAGER_QUEUE_FAILURE),
              "Failed to insert command index %ld", i);
        }
      }

      // Only change if the current status was false.
      if (transaction->current_phase_failed.compare_exchange_strong(failed,
                                                                    true)) {
        transaction->current_phase_execution_result = execution_result;
      }

      failed_operations++;
    }
  }

  if (failed_operations > 0 && transaction->pending_callbacks.fetch_sub(
                                   failed_operations) == failed_operations) {
    ProceedToNextPhase(current_phase, transaction);
  }
}

ExecutionResult TransactionEngine::DispatchDistributedCommand(
    size_t command_index, TransactionPhase current_phase,
    shared_ptr<TransactionCommand>& command,
    shared_ptr<Transaction>& transaction) noexcept {
  TransactionCommandCallback transaction_callback =
      bind(&TransactionEngine::OnPhaseCallback, this, command_index,
           current_phase, _1, transaction);
  if (current_phase == TransactionPhase::Begin) {
    return command->begin(transaction_callback);
  }
  if (current_phase == TransactionPhase::Prepare) {
    return command->prepare(transaction_callback);
  }
  if (current_phase == TransactionPhase::Commit) {
    return command->commit(transaction_callback);
  }
  if (current_phase == TransactionPhase::CommitNotify) {
    return command->notify(transaction_callback);
  }
  if (current_phase == TransactionPhase::AbortNotify) {
    return command->abort(transaction_callback);
  }
  if (current_phase == TransactionPhase::End) {
    return command->end(transaction_callback);
  }

  return FailureExecutionResult(
      errors::SC_TRANSACTION_ENGINE_INVALID_DISTRIBUTED_COMMAND);
}

void TransactionEngine::OnPhaseCallback(
    size_t command_index, TransactionPhase current_phase,
    ExecutionResult& execution_result,
    std::shared_ptr<Transaction>& transaction) noexcept {
  // All the retries must have happened before the TM. TM is not responsible
  // to retry the operation.
  if (execution_result.status == ExecutionStatus::Retry) {
    ALERT_CONTEXT_WITH_TRANSACTION_SCP_INFO(
        transaction->context, transaction,
        FailureExecutionResult(
            errors::SC_TRANSACTION_MANAGER_TRANSACTION_STUCK),
        "The transaction is stuck!");
    return;
  }

  if (execution_result.status == ExecutionStatus::Failure) {
    auto failed = false;
    auto enqueue_execution_result =
        transaction->current_phase_failed_command_indices.TryEnqueue(
            command_index);
    if (!enqueue_execution_result.Successful()) {
      ERROR_CONTEXT_WITH_TRANSACTION_SCP_INFO(
          transaction->context, transaction,
          FailureExecutionResult(errors::SC_TRANSACTION_MANAGER_QUEUE_FAILURE),
          "Failed to insert command index %ld", command_index);
    }

    // Only change if the current status was false.
    if (transaction->current_phase_failed.compare_exchange_strong(failed,
                                                                  true)) {
      transaction->current_phase_execution_result = execution_result;
    }
  }

  // Was it the last callback?
  if (transaction->pending_callbacks.fetch_sub(1) != 1) {
    return;
  }

  ProceedToNextPhase(current_phase, transaction);
}

transaction_manager::TransactionPhase
TransactionEngine::ConvertProtoPhaseToPhase(
    transaction_manager::proto::TransactionPhase phase) noexcept {
  switch (phase) {
    case transaction_manager::proto::TransactionPhase::NOT_STARTED:
      return transaction_manager::TransactionPhase::NotStarted;
    case transaction_manager::proto::TransactionPhase::BEGIN:
      return transaction_manager::TransactionPhase::Begin;
    case transaction_manager::proto::TransactionPhase::PREPARE:
      return transaction_manager::TransactionPhase::Prepare;
    case transaction_manager::proto::TransactionPhase::COMMIT:
      return transaction_manager::TransactionPhase::Commit;
    case transaction_manager::proto::TransactionPhase::COMMIT_NOTIFY:
      return transaction_manager::TransactionPhase::CommitNotify;
    case transaction_manager::proto::TransactionPhase::COMMITTED:
      return transaction_manager::TransactionPhase::Committed;
    case transaction_manager::proto::TransactionPhase::ABORT_NOTIFY:
      return transaction_manager::TransactionPhase::AbortNotify;
    case transaction_manager::proto::TransactionPhase::ABORTED:
      return transaction_manager::TransactionPhase::Aborted;
    case transaction_manager::proto::TransactionPhase::END:
      return transaction_manager::TransactionPhase::End;
    case transaction_manager::proto::TransactionPhase::
        TRANSACTION_PHASE_TYPE_UNKNOWN:
    default:
      return transaction_manager::TransactionPhase::Unknown;
  }
}

transaction_manager::proto::TransactionPhase
TransactionEngine::ConvertPhaseToProtoPhase(
    transaction_manager::TransactionPhase phase) noexcept {
  switch (phase) {
    case transaction_manager::TransactionPhase::NotStarted:
      return transaction_manager::proto::TransactionPhase::NOT_STARTED;
    case transaction_manager::TransactionPhase::Begin:
      return transaction_manager::proto::TransactionPhase::BEGIN;
    case transaction_manager::TransactionPhase::Prepare:
      return transaction_manager::proto::TransactionPhase::PREPARE;
    case transaction_manager::TransactionPhase::Commit:
      return transaction_manager::proto::TransactionPhase::COMMIT;
    case transaction_manager::TransactionPhase::CommitNotify:
      return transaction_manager::proto::TransactionPhase::COMMIT_NOTIFY;
    case transaction_manager::TransactionPhase::Committed:
      return transaction_manager::proto::TransactionPhase::COMMITTED;
    case transaction_manager::TransactionPhase::AbortNotify:
      return transaction_manager::proto::TransactionPhase::ABORT_NOTIFY;
    case transaction_manager::TransactionPhase::Aborted:
      return transaction_manager::proto::TransactionPhase::ABORTED;
    case transaction_manager::TransactionPhase::End:
      return transaction_manager::proto::TransactionPhase::END;
    case transaction_manager::TransactionPhase::Unknown:
    default:
      return transaction_manager::proto::TransactionPhase::
          TRANSACTION_PHASE_TYPE_UNKNOWN;
  }
}

std::string TransactionEngine::TransactionPhaseToString(
    transaction_manager::TransactionPhase phase) {
  switch (phase) {
    case transaction_manager::TransactionPhase::NotStarted:
      return kTransactionPhaseNotStartedStr;
    case transaction_manager::TransactionPhase::Begin:
      return kTransactionPhaseBeginStr;
    case transaction_manager::TransactionPhase::Prepare:
      return kTransactionPhasePrepareStr;
    case transaction_manager::TransactionPhase::Commit:
      return kTransactionPhaseCommitStr;
    case transaction_manager::TransactionPhase::CommitNotify:
      return kTransactionPhaseCommitNotifyStr;
    case transaction_manager::TransactionPhase::Committed:
      return kTransactionPhaseCommittedStr;
    case transaction_manager::TransactionPhase::AbortNotify:
      return kTransactionPhaseAbortNotifyStr;
    case transaction_manager::TransactionPhase::Aborted:
      return kTransactionPhaseAbortedStr;
    case transaction_manager::TransactionPhase::End:
      return kTransactionPhaseEndStr;
    case transaction_manager::TransactionPhase::Unknown:
    default:
      return kTransactionPhaseUnknownStr;
  }
}

std::string TransactionEngine::TransactionExecutionPhaseToString(
    TransactionExecutionPhase phase) {
  switch (phase) {
    case TransactionExecutionPhase::Begin:
      return kTransactionPhaseBeginStr;
    case TransactionExecutionPhase::Prepare:
      return kTransactionPhasePrepareStr;
    case TransactionExecutionPhase::Commit:
      return kTransactionPhaseCommitStr;
    case TransactionExecutionPhase::Notify:
      return kTransactionPhaseCommitNotifyStr;
    case TransactionExecutionPhase::Abort:
      return kTransactionPhaseAbortNotifyStr;
    case TransactionExecutionPhase::End:
      return kTransactionPhaseEndStr;
    case TransactionExecutionPhase::Unknown:
    default:
      return kTransactionPhaseUnknownStr;
  }
}

TransactionExecutionPhase TransactionEngine::ToTransactionExecutionPhase(
    transaction_manager::TransactionPhase phase) noexcept {
  switch (phase) {
    case transaction_manager::TransactionPhase::NotStarted:
      return TransactionExecutionPhase::Begin;
    case transaction_manager::TransactionPhase::Begin:
      return TransactionExecutionPhase::Begin;
    case transaction_manager::TransactionPhase::Prepare:
      return TransactionExecutionPhase::Prepare;
    case transaction_manager::TransactionPhase::Commit:
      return TransactionExecutionPhase::Commit;
    case transaction_manager::TransactionPhase::CommitNotify:
      return TransactionExecutionPhase::Notify;
    case transaction_manager::TransactionPhase::Committed:
      return TransactionExecutionPhase::End;
    case transaction_manager::TransactionPhase::AbortNotify:
      return TransactionExecutionPhase::Abort;
    case transaction_manager::TransactionPhase::Aborted:
      return TransactionExecutionPhase::End;
    case transaction_manager::TransactionPhase::End:
      return TransactionExecutionPhase::End;
    case transaction_manager::TransactionPhase::Unknown:
    default:
      return TransactionExecutionPhase::Unknown;
  }
}

transaction_manager::TransactionPhase TransactionEngine::ToTransactionPhase(
    TransactionExecutionPhase phase) noexcept {
  switch (phase) {
    case TransactionExecutionPhase::Begin:
      return transaction_manager::TransactionPhase::Begin;
    case TransactionExecutionPhase::Prepare:
      return transaction_manager::TransactionPhase::Prepare;
    case TransactionExecutionPhase::Commit:
      return transaction_manager::TransactionPhase::Commit;
    case TransactionExecutionPhase::Notify:
      return transaction_manager::TransactionPhase::CommitNotify;
    case TransactionExecutionPhase::Abort:
      return transaction_manager::TransactionPhase::AbortNotify;
    case TransactionExecutionPhase::End:
      return transaction_manager::TransactionPhase::End;
    case TransactionExecutionPhase::Unknown:
    default:
      return transaction_manager::TransactionPhase::Unknown;
  }
}

bool TransactionEngine::LocalAndRemoteTransactionsInSync(
    TransactionPhase transaction_phase,
    TransactionPhase remote_transaction_phase) noexcept {
  // When getting the status, there are 3 possibilities:
  // 1) Current transaction is ahead of the remote transaction.
  // 2) Current transaction is behind of the remote transaction.
  // 3) Both are in the same phase.
  if (transaction_phase == TransactionPhase::Begin) {
    if (remote_transaction_phase == TransactionPhase::NotStarted ||
        remote_transaction_phase == TransactionPhase::Begin ||
        remote_transaction_phase == TransactionPhase::Prepare ||
        remote_transaction_phase == TransactionPhase::AbortNotify) {
      return true;
    }
  }

  if (transaction_phase == TransactionPhase::Prepare) {
    if (remote_transaction_phase == TransactionPhase::Begin ||
        remote_transaction_phase == TransactionPhase::Prepare ||
        remote_transaction_phase == TransactionPhase::Commit ||
        remote_transaction_phase == TransactionPhase::AbortNotify) {
      return true;
    }
  }

  if (transaction_phase == TransactionPhase::Commit) {
    if (remote_transaction_phase == TransactionPhase::Prepare ||
        remote_transaction_phase == TransactionPhase::Commit ||
        remote_transaction_phase == TransactionPhase::CommitNotify ||
        remote_transaction_phase == TransactionPhase::AbortNotify) {
      return true;
    }
  }

  if (transaction_phase == TransactionPhase::CommitNotify) {
    if (remote_transaction_phase == TransactionPhase::Commit ||
        remote_transaction_phase == TransactionPhase::CommitNotify ||
        remote_transaction_phase == TransactionPhase::AbortNotify ||
        remote_transaction_phase == TransactionPhase::Committed ||
        remote_transaction_phase == TransactionPhase::End) {
      return true;
    }
  }

  if (transaction_phase == TransactionPhase::AbortNotify) {
    if (remote_transaction_phase == TransactionPhase::Commit ||
        remote_transaction_phase == TransactionPhase::CommitNotify ||
        remote_transaction_phase == TransactionPhase::AbortNotify ||
        remote_transaction_phase == TransactionPhase::Aborted ||
        remote_transaction_phase == TransactionPhase::End) {
      return true;
    }
  }

  if (transaction_phase == TransactionPhase::Committed) {
    if (remote_transaction_phase == TransactionPhase::CommitNotify ||
        remote_transaction_phase == TransactionPhase::Committed ||
        remote_transaction_phase == TransactionPhase::End) {
      return true;
    }
  }

  if (transaction_phase == TransactionPhase::Aborted) {
    if (remote_transaction_phase == TransactionPhase::AbortNotify ||
        remote_transaction_phase == TransactionPhase::Aborted ||
        remote_transaction_phase == TransactionPhase::End) {
      return true;
    }
  }

  if (transaction_phase == TransactionPhase::End) {
    if (remote_transaction_phase == TransactionPhase::Committed ||
        remote_transaction_phase == TransactionPhase::Aborted ||
        remote_transaction_phase == TransactionPhase::End) {
      return true;
    }
  }

  return false;
}

bool TransactionEngine::CanCancel(TransactionPhase current_phase) noexcept {
  // Transaction in AbortNotify can not be cancelled because it has already
  // been aborted for a reason, so cancellation is unneeded.
  if (current_phase == TransactionPhase::Committed ||
      current_phase == TransactionPhase::AbortNotify ||
      current_phase == TransactionPhase::Aborted ||
      current_phase == TransactionPhase::End ||
      current_phase == TransactionPhase::Unknown) {
    return false;
  }

  return true;
}

size_t TransactionEngine::GetPendingTransactionCount() noexcept {
  return active_transactions_map_.Size();
}
}  // namespace google::scp::core
