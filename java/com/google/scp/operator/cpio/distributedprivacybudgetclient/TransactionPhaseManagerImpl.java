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

/** Transaction phase manager acts as a state machine for executing the transactions. */
public class TransactionPhaseManagerImpl implements TransactionPhaseManager {

  /**
   * Provides the next state on the transaction state machine based on the current phase and the
   * current phase execution result.
   *
   * @param currentPhase The current phase of the transaction.
   * @param currentPhaseResult The current phase execution result of the transaction.
   * @return TransactionPhase The next transaction phase.
   */
  @Override
  public TransactionPhase proceedToNextPhase(
      TransactionPhase currentPhase, ExecutionResult currentPhaseResult) {
    if (currentPhase == TransactionPhase.UNKNOWN) {
      return TransactionPhase.ABORT;
    }
    if (currentPhaseResult.executionStatus() == ExecutionStatus.RETRY) {
      return currentPhase;
    }
    boolean currentPhaseSucceeded =
        (currentPhaseResult.executionStatus() != ExecutionStatus.FAILURE);
    currentPhase = proceedToNextPhaseInternal(currentPhase, currentPhaseSucceeded);
    return currentPhase;
  }

  private TransactionPhase proceedToNextPhaseInternal(
      TransactionPhase currentPhase, boolean currentPhaseSucceeded) {
    switch (currentPhase) {
      case NOTSTARTED:
        return currentPhaseSucceeded ? TransactionPhase.BEGIN : TransactionPhase.END;
      case BEGIN:
        return currentPhaseSucceeded ? TransactionPhase.PREPARE : TransactionPhase.ABORT;
      case PREPARE:
        return currentPhaseSucceeded ? TransactionPhase.COMMIT : TransactionPhase.END;
      case COMMIT:
        return currentPhaseSucceeded ? TransactionPhase.NOTIFY : TransactionPhase.ABORT;
      case NOTIFY:
        return currentPhaseSucceeded ? TransactionPhase.END : TransactionPhase.NOTIFY;
      case ABORT:
        return currentPhaseSucceeded ? TransactionPhase.END : TransactionPhase.ABORT;
      case END:
        return TransactionPhase.FINISHED;
      default:
        return TransactionPhase.UNKNOWN;
    }
  }
}
