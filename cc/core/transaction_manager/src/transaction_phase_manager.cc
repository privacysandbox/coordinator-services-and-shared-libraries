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

#include "transaction_phase_manager.h"

using google::scp::core::transaction_manager::TransactionPhase;

namespace google::scp::core {
TransactionPhase TransactionPhaseManager::ProceedToNextPhase(
    TransactionPhase current_phase,
    ExecutionResult current_phase_result) noexcept {
  if (current_phase == TransactionPhase::Unknown) {
    return TransactionPhase::Aborted;
  }

  if (current_phase_result ==
      RetryExecutionResult(current_phase_result.status_code)) {
    return current_phase;
  }

  auto current_phase_succeeded = true;
  if (current_phase_result ==
      FailureExecutionResult(current_phase_result.status_code)) {
    current_phase_succeeded = false;
  }

  current_phase =
      ProceedToNextPhaseInternal(current_phase, current_phase_succeeded);
  return current_phase;
};

TransactionPhase TransactionPhaseManager::ProceedToNextPhaseInternal(
    TransactionPhase current_phase, bool current_phase_succeeded) noexcept {
  switch (current_phase) {
    case TransactionPhase::NotStarted:
      return current_phase_succeeded ? TransactionPhase::Begin
                                     : TransactionPhase::End;
    case TransactionPhase::Begin:
      return current_phase_succeeded ? TransactionPhase::Prepare
                                     : TransactionPhase::Aborted;
    case TransactionPhase::Prepare:
      return current_phase_succeeded ? TransactionPhase::Commit
                                     : TransactionPhase::Aborted;
    case TransactionPhase::Commit:
      return current_phase_succeeded ? TransactionPhase::CommitNotify
                                     : TransactionPhase::AbortNotify;
    case TransactionPhase::CommitNotify:
      return current_phase_succeeded ? TransactionPhase::Committed
                                     : TransactionPhase::Unknown;
    case TransactionPhase::Committed:
      return current_phase_succeeded ? TransactionPhase::End
                                     : TransactionPhase::Unknown;
    case TransactionPhase::AbortNotify:
      return current_phase_succeeded ? TransactionPhase::Aborted
                                     : TransactionPhase::Unknown;
    case TransactionPhase::Aborted:
      return current_phase_succeeded ? TransactionPhase::End
                                     : TransactionPhase::Unknown;
    default:
      return TransactionPhase::Unknown;
  }
}

bool TransactionPhaseManager::CanProceedToAbortAtPhase(
    TransactionPhase transaction_phase) {
  // NOTE:
  // For BEGIN and PREPARE, although transaction can proceed to ABORT NOTIFY and
  // then eventually END phase, ideally the transaction SHOULD proceed to END
  // directly since the transaction did not prepare anything on
  // Resources/Resource Managers and executing ABORT NOTIFY phase is
  // unnecessary.
  return (transaction_phase == TransactionPhase::Begin ||
          transaction_phase == TransactionPhase::Prepare ||
          transaction_phase == TransactionPhase::Commit ||
          transaction_phase == TransactionPhase::CommitNotify ||
          transaction_phase == TransactionPhase::AbortNotify);
}

bool TransactionPhaseManager::CanProceedToEndAtPhase(
    TransactionPhase transaction_phase) {
  // If current phase is COMMIT phase which could either mean that COMMIT phase
  // has not started or started but not completed successfully on all
  // resource managers, so we should be pessimistic and clean up any modified
  // state by proceeding to ABORT NOTIFY instead of proceeding to END phase
  // directly.
  return (transaction_phase == TransactionPhase::Begin ||
          transaction_phase == TransactionPhase::Prepare ||
          transaction_phase == TransactionPhase::Aborted ||
          transaction_phase == TransactionPhase::Committed ||
          transaction_phase == TransactionPhase::End);
}

}  // namespace google::scp::core
