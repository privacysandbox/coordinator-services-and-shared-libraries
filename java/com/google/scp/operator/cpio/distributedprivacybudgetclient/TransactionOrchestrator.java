/*
 * Copyright 2024 Google LLC
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
import com.google.scp.coordinator.privacy.budgeting.model.ReportingOriginToPrivacyBudgetUnits;

/** Orchestrates transactions across coordinators. */
public interface TransactionOrchestrator {

  /**
   * Executes endpoints like HEALTH-CHECK and CONSUME-BUDGET on all coordinators.
   *
   * @param transactionRequest Transaction request data.
   * @return A list of exhausted privacy budget units.
   * @throws TransactionOrchestratorException if any calls to HEALTH-CHECK or CONSUME-BUDGET fail.
   */
  ImmutableList<ReportingOriginToPrivacyBudgetUnits> execute(TransactionRequest transactionRequest)
      throws TransactionOrchestratorException;

  /** Represents an exception thrown by {@link TransactionOrchestrator}. */
  final class TransactionOrchestratorException extends Exception {
    private final StatusCode statusCode;

    /** Creates a new instance of the exception with the specified {@link StatusCode}. */
    public TransactionOrchestratorException(StatusCode statusCode) {
      super(statusCode.name());
      this.statusCode = statusCode;
    }

    public StatusCode getStatusCode() {
      return this.statusCode;
    }
  }
}
