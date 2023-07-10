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

public interface PrivacyBudgetClient {

  /**
   * Performs request against the provided baseUrl as part of BEGIN phase of transaction
   *
   * @param transaction -The transaction which the action pertains to
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult performActionBegin(Transaction transaction) throws PrivacyBudgetClientException;

  /**
   * Performs request against the provided baseUrl as part of PREPARE phase of transaction
   *
   * @param transaction -The transaction which the action pertains to
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult performActionPrepare(Transaction transaction) throws PrivacyBudgetClientException;

  /**
   * Performs request against the provided baseUrl as part of COMMIT phase of transaction
   *
   * @param transaction -The transaction which the action pertains to
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult performActionCommit(Transaction transaction) throws PrivacyBudgetClientException;

  /**
   * Performs request against the provided baseUrl as part of NOTIFY phase of transaction
   *
   * @param transaction -The transaction which the action pertains to
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult performActionNotify(Transaction transaction) throws PrivacyBudgetClientException;

  /**
   * Performs request against the provided baseUrl as part of END phase of transaction
   *
   * @param transaction -The transaction which the action pertains to
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult performActionEnd(Transaction transaction) throws PrivacyBudgetClientException;

  /**
   * Performs request against the provided baseUrl as part of ABORT phase of transaction
   *
   * @param transaction -The transaction which the action pertains to
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult performActionAbort(Transaction transaction) throws PrivacyBudgetClientException;

  /**
   * Returns the Base URL of the coordinator against which this client performs the transaction
   * actions.
   *
   * @return
   */
  String getPrivacyBudgetServerIdentifier();

  /** Represents an exception thrown by PrivacyBudgetClient */
  final class PrivacyBudgetClientException extends Exception {
    /** Creates a new instance of the exception. */
    public PrivacyBudgetClientException(String message) {
      super(message);
    }
  }
}
