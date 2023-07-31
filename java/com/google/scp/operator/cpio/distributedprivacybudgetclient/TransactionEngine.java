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

import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;

/** Interface for the transaction engine which is responsible to execute the transactions. */
public interface TransactionEngine {

  /**
   * Executes a transaction
   *
   * @param request Transaction request metadata.
   */
  ImmutableList<PrivacyBudgetUnit> execute(TransactionRequest request)
      throws TransactionEngineException;

  /**
   * To execute a transaction phase, the caller must provide the transaction id and the desired
   * phase to be executed.
   *
   * @param transactionPhaseRequest Transaction phase request metadata.
   */
  void executePhase(TransactionPhaseRequest transactionPhaseRequest)
      throws TransactionEngineException;

  /** Represents an exception thrown by TransactionEngine */
  final class TransactionEngineException extends Exception {
    private final StatusCode statusCode;

    /** Creates a new instance of the exception. */
    public TransactionEngineException(StatusCode statusCode) {
      super(statusCode.name());
      this.statusCode = statusCode;
    }

    /** Creates a new instance of the exception. */
    public TransactionEngineException(StatusCode statusCode, Throwable cause) {
      super(statusCode.name(), cause);
      this.statusCode = statusCode;
    }

    public StatusCode getStatusCode() {
      return this.statusCode;
    }
  }
}
