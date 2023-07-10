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

#include <string_view>

#include "public/core/interface/execution_result.h"

namespace google::scp::core::transaction_manager {

enum class TransactionPhase {
  NotStarted = 0,
  Begin = 1,
  Prepare = 2,
  Commit = 3,
  CommitNotify = 4,
  Committed = 5,
  AbortNotify = 6,
  Aborted = 7,
  End = 8,
  Unknown = 1000
};

/// Is responsible to act as a state machine for the execution of transactions.
class TransactionPhaseManagerInterface {
 public:
  virtual ~TransactionPhaseManagerInterface() = default;
  /**
   * @brief Provides the next state on the transaction state machine based
   * on the current phase and the current phase execution result.
   *
   * @param current_phase The current phase of the transaction.
   * @param current_phase_result The current phase execution result of the
   * transaction.
   * @return TransactionPhase The next transaction phase.
   */
  virtual TransactionPhase ProceedToNextPhase(
      TransactionPhase current_phase,
      ExecutionResult current_phase_result) noexcept = 0;
};
}  // namespace google::scp::core::transaction_manager
