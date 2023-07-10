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
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

/**
 * Transaction Is used as a common context between different phases of each transaction. This object
 * will be cached into a map and transaction engine can access it at any time.
 */
public class Transaction {
  private static final int DEFAULT_RETRIES = 5;
  // The current transaction id.
  private UUID id;
  // The context of the transaction.
  private TransactionRequest request;
  // The current phase of the transaction.
  private TransactionPhase currentPhase;
  // The current phase execution result of the transaction.
  private ExecutionResult currentPhaseExecutionResult;
  // The execution result of the transaction.
  private ExecutionResult transactionExecutionResult;
  // Indicates whether the transaction phase failed.
  private boolean transactionPhaseFailed;
  // Indicates whether the transaction failed.
  private boolean transactionFailed;
  // Indicates whether the current phase failed.
  private boolean currentPhaseFailed;
  // Last execution timestamp against every BaseUrl for any phases of the current transaction to
  // support optimistic concurrency.
  private Map<String, String> lastExecutionTimeNanosForBaseUrl;

  private Map<String, TransactionPhase> lastCompletedTransactionPhaseOnPrivacyBudgetServer;
  // Number of retries before returning failure.
  private int retries;
  // List of privacy budget units whose budget has exhausted
  private ImmutableList<PrivacyBudgetUnit> exhaustedPrivacyBudgetUnits;

  Transaction() {
    id = new UUID(0L, 0L);
    currentPhase = TransactionPhase.NOTSTARTED;
    currentPhaseExecutionResult = ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK);
    transactionExecutionResult =
        ExecutionResult.create(
            ExecutionStatus.FAILURE, StatusCode.TRANSACTION_MANAGER_NOT_FINISHED);
    transactionPhaseFailed = false;
    transactionFailed = false;
    currentPhaseFailed = false;
    lastExecutionTimeNanosForBaseUrl = new HashMap<>();
    lastCompletedTransactionPhaseOnPrivacyBudgetServer = new HashMap<>();
    retries = DEFAULT_RETRIES;
    exhaustedPrivacyBudgetUnits = ImmutableList.of();
  }

  public UUID getId() {
    return this.id;
  }

  public void setId(UUID uuid) {
    this.id = uuid;
  }

  public TransactionRequest getRequest() {
    return request;
  }

  public void setRequest(TransactionRequest request) {
    this.request = request;
  }

  public boolean isTransactionPhaseFailed() {
    return transactionPhaseFailed;
  }

  public void setTransactionPhaseFailed(boolean transactionPhaseFailed) {
    this.transactionPhaseFailed = transactionPhaseFailed;
  }

  public boolean isTransactionFailed() {
    return transactionFailed;
  }

  public void setTransactionFailed(boolean transactionFailed) {
    this.transactionFailed = transactionFailed;
  }

  ExecutionResult getTransactionExecutionResult() {
    return this.transactionExecutionResult;
  }

  public void setTransactionExecutionResult(ExecutionResult transactionExecutionResult) {
    this.transactionExecutionResult = transactionExecutionResult;
  }

  public TransactionPhase getCurrentPhase() {
    return currentPhase;
  }

  public void setCurrentPhase(TransactionPhase currentPhase) {
    this.currentPhase = currentPhase;
  }

  public ExecutionResult getCurrentPhaseExecutionResult() {
    return currentPhaseExecutionResult;
  }

  public void setCurrentPhaseExecutionResult(ExecutionResult currentPhaseExecutionResult) {
    this.currentPhaseExecutionResult = currentPhaseExecutionResult;
  }

  public boolean isCurrentPhaseFailed() {
    return currentPhaseFailed;
  }

  public void setCurrentPhaseFailed(boolean currentPhaseFailed) {
    this.currentPhaseFailed = currentPhaseFailed;
  }

  public TransactionPhase getLastCompletedTransactionPhaseOnPrivacyBudgetServer(String baseUrl) {
    return lastCompletedTransactionPhaseOnPrivacyBudgetServer.getOrDefault(
        baseUrl, TransactionPhase.NOTSTARTED);
  }

  public void setLastCompletedTransactionPhaseOnPrivacyBudgetServer(
      String baseUrl, TransactionPhase transactionPhase) {
    lastCompletedTransactionPhaseOnPrivacyBudgetServer.put(baseUrl, transactionPhase);
  }

  public String getLastExecutionTimestamp(String baseUrl) {
    return lastExecutionTimeNanosForBaseUrl.get(baseUrl);
  }

  public void setLastExecutionTimestamp(String baseUrl, String lastExecutionTimestamp) {
    lastExecutionTimeNanosForBaseUrl.put(baseUrl, lastExecutionTimestamp);
  }

  public int getRetries() {
    return retries;
  }

  public void setRetries(int retries) {
    this.retries = retries;
  }

  public void resetRetries() {
    this.retries = DEFAULT_RETRIES;
  }

  public ImmutableList<PrivacyBudgetUnit> getExhaustedPrivacyBudgetUnits() {
    return exhaustedPrivacyBudgetUnits;
  }

  public void setExhaustedPrivacyBudgetUnits(
      ImmutableList<PrivacyBudgetUnit> exhaustedPrivacyBudgetUnits) {
    this.exhaustedPrivacyBudgetUnits = exhaustedPrivacyBudgetUnits;
  }
}
