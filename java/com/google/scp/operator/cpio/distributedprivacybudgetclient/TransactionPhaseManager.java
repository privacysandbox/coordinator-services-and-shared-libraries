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

package com.google.scp.operator.cpio.distributedprivacybudgetclient;

/**
 * Interface for transaction phase manager which is responsible to act as a state machine for the
 * execution of transactions.
 */
public interface TransactionPhaseManager {
  /**
   * Provides the next state on the transaction state machine based on the current phase and the
   * current phase execution result.
   *
   * @param currentPhase The current phase of the transaction.
   * @param currentPhaseResult The current phase execution result of the transaction.
   * @return TransactionPhase The next transaction phase.
   */
  TransactionPhase proceedToNextPhase(
      TransactionPhase currentPhase, ExecutionResult currentPhaseResult);
}
