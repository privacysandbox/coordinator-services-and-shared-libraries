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
import com.google.inject.Inject;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.PrivacyBudgetClient.PrivacyBudgetClientException;
import java.util.Map;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Transaction engine is responsible for running the transactions. */
public class TransactionEngineImpl implements TransactionEngine {

  /* The Key specifies the phase we can ignore failures against if the phase has succeeded at lest of the number of coordinators specified by Value */
  private static final Map<TransactionPhase, Integer> PARTIAL_SUCCESS_ACCEPTABLE_PHASES =
      Map.of(TransactionPhase.NOTIFY, 1, TransactionPhase.END, 0);
  private static final Logger logger = LoggerFactory.getLogger(TransactionEngineImpl.class);
  private final TransactionPhaseManager transactionPhaseManager;
  private final ImmutableList<PrivacyBudgetClient> privacyBudgetClients;

  /** Creates a new instance of the {@code TransactionEngineImpl} class. */
  @Inject
  public TransactionEngineImpl(
      TransactionPhaseManager transactionPhaseManager,
      ImmutableList<PrivacyBudgetClient> privacyBudgetClients) {
    this.transactionPhaseManager = transactionPhaseManager;
    this.privacyBudgetClients = privacyBudgetClients;
  }

  private Transaction initializeTransaction(TransactionRequest request) {
    Transaction transaction = new Transaction();
    transaction.setId(request.transactionId());
    transaction.setRequest(request);
    return transaction;
  }

  private void proceedToNextPhase(TransactionPhase currentPhase, Transaction transaction)
      throws TransactionEngineException {
    TransactionPhase nextPhase =
        transactionPhaseManager.proceedToNextPhase(
            currentPhase, transaction.getCurrentPhaseExecutionResult());

    /* This means that END state was executed either successfully or with failures. However, that does
     * not affect the eventual outcome of the transaction. What decides whether the budget consumption
     *  was successful or not depends on if ABORT was invoked. If it was, that means the transaction
     *  failed and it has been marked as such in the Transaction object
     * */
    if (nextPhase == TransactionPhase.FINISHED) {
      if (transaction.isTransactionFailed()) {
        StatusCode statusCode = transaction.getTransactionExecutionResult().statusCode();
        throw createTransactionEngineException(
            statusCode, StatusCode.TRANSACTION_ENGINE_TRANSACTION_FAILED);
      }
      return;
    }

    if (nextPhase == TransactionPhase.ABORT) {
      transaction.setTransactionFailed(true);
    }

    if (nextPhase == TransactionPhase.UNKNOWN) {
      if (currentPhase == TransactionPhase.BEGIN) {
        StatusCode statusCode = transaction.getCurrentPhaseExecutionResult().statusCode();
        throw createTransactionEngineException(
            statusCode, StatusCode.TRANSACTION_ENGINE_CANNOT_START_TRANSACTION);
      }
      throw new TransactionEngineException(StatusCode.TRANSACTION_MANAGER_TRANSACTION_UNKNOWN);
    }

    // Handle maximum retries for Transaction phase
    if (nextPhase == currentPhase) {
      transaction.setRetries(transaction.getRetries() - 1);
    } else {
      transaction.resetRetries();
    }

    if (transaction.getRetries() <= 0) {
      /* In case it was NOTIFY that failed,
       * we might still be able to declare success if the failure was only partial.
       * */
      if (canMarkTransactionAsSuccessful(currentPhase, transaction)) {
        return;
      }
      throw new TransactionEngineException(StatusCode.TRANSACTION_MANAGER_RETRIES_EXCEEDED);
    }

    if (transaction.isCurrentPhaseFailed()) {
      // Only change if the current status was false.
      if (!transaction.isTransactionPhaseFailed()) {
        transaction.setTransactionPhaseFailed(true);
        transaction.setTransactionExecutionResult(transaction.getCurrentPhaseExecutionResult());
      }
      // Reset the state before continuing to the next phase.
      transaction.setCurrentPhaseFailed(false);
      transaction.setCurrentPhaseExecutionResult(
          ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK));
    }

    if (transaction.getCurrentPhase() != currentPhase) {
      return;
    }
    transaction.setCurrentPhase(nextPhase);
    executeCurrentPhase(transaction);
  }

  /* It is ok to ignore failures against certain coordinators for some phases if the phase has
   * succeeded against at least a minimum required number of coordinators for that phase.
   * */
  private boolean canMarkTransactionAsSuccessful(TransactionPhase phase, Transaction transaction) {
    if (PARTIAL_SUCCESS_ACCEPTABLE_PHASES.containsKey(phase)) {
      long phaseSucceededCount = getCountOfPhaseSuccessAgainstCoordinators(transaction, phase);
      if (phaseSucceededCount >= PARTIAL_SUCCESS_ACCEPTABLE_PHASES.get(phase)) {
        return true;
      }
    }
    return false;
  }

  private void executeCurrentPhase(Transaction transaction) throws TransactionEngineException {
    switch (transaction.getCurrentPhase()) {
      case BEGIN:
        executeDistributedPhase(TransactionPhase.BEGIN, transaction);
        return;
      case PREPARE:
        executeDistributedPhase(TransactionPhase.PREPARE, transaction);
        return;
      case COMMIT:
        executeDistributedPhase(TransactionPhase.COMMIT, transaction);
        return;
      case NOTIFY:
        executeDistributedPhase(TransactionPhase.NOTIFY, transaction);
        return;
      case ABORT:
        executeDistributedPhase(TransactionPhase.ABORT, transaction);
        return;
      case END:
        executeDistributedPhase(TransactionPhase.END, transaction);
        return;
      default:
        throw new TransactionEngineException(
            StatusCode.TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE);
    }
  }

  private void executeDistributedPhase(TransactionPhase currentPhase, Transaction transaction)
      throws TransactionEngineException {
    for (PrivacyBudgetClient privacyBudgetClient : this.privacyBudgetClients) {
      String privacyBudgetServerIdentifier = privacyBudgetClient.getPrivacyBudgetServerIdentifier();
      TransactionPhase lastSuccessfulTransactionPhase =
          transaction.getLastCompletedTransactionPhaseOnPrivacyBudgetServer(
              privacyBudgetServerIdentifier);
      if ((lastSuccessfulTransactionPhase == TransactionPhase.NOTSTARTED
              && currentPhase != TransactionPhase.BEGIN)
          || lastSuccessfulTransactionPhase == TransactionPhase.END) {
        continue;
      }
      logger.info(
          "[{}] Executing phase '{}' against coordinator: '{}'",
          transaction.getId(),
          currentPhase,
          privacyBudgetServerIdentifier);
      ExecutionResult executionResult =
          dispatchDistributedCommand(privacyBudgetClient, transaction);
      if (executionResult.executionStatus() != ExecutionStatus.SUCCESS) {
        // Only change if the current status was false.
        if (!transaction.isCurrentPhaseFailed()) {
          logger.info(
              "[{}] Phase '{}' against coordinator: '{}' failed",
              transaction.getId(),
              currentPhase,
              privacyBudgetServerIdentifier);
          transaction.setCurrentPhaseFailed(true);
          transaction.setCurrentPhaseExecutionResult(executionResult);
        }
      } else {
        transaction.setLastCompletedTransactionPhaseOnPrivacyBudgetServer(
            privacyBudgetServerIdentifier, currentPhase);
      }
    }
    proceedToNextPhase(currentPhase, transaction);
  }

  private static TransactionEngineException createTransactionEngineException(
      StatusCode transactionStatusCode, StatusCode defaultStatusCode) {
    if (transactionStatusCode == StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHENTICATED
        || transactionStatusCode == StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHORIZED) {
      return new TransactionEngineException(transactionStatusCode);
    }
    return new TransactionEngineException(defaultStatusCode);
  }

  private ExecutionResult dispatchDistributedCommand(
      PrivacyBudgetClient privacyBudgetClient, Transaction transaction)
      throws TransactionEngineException {
    try {
      switch (transaction.getCurrentPhase()) {
        case BEGIN:
          return privacyBudgetClient.performActionBegin(transaction);
        case PREPARE:
          return privacyBudgetClient.performActionPrepare(transaction);

        case COMMIT:
          return privacyBudgetClient.performActionCommit(transaction);

        case NOTIFY:
          return privacyBudgetClient.performActionNotify(transaction);

        case ABORT:
          return privacyBudgetClient.performActionAbort(transaction);

        case END:
          return privacyBudgetClient.performActionEnd(transaction);

        default:
          throw new TransactionEngineException(
              StatusCode.TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE);
      }
    } catch (PrivacyBudgetClientException e) {
      logger.error(
          "[{}] Failed to perform transaction phase action. Error is: ",
          transaction.getId(),
          e.getMessage());
      throw new TransactionEngineException(StatusCode.UNKNOWN, e);
    }
  }

  private long getCountOfPhaseSuccessAgainstCoordinators(
      Transaction transaction, TransactionPhase transactionPhase) {
    return privacyBudgetClients.stream()
        .map(PrivacyBudgetClient::getPrivacyBudgetServerIdentifier)
        .map(
            identifier ->
                transaction.getLastCompletedTransactionPhaseOnPrivacyBudgetServer(identifier))
        .filter(lastSuccessfulPhase -> lastSuccessfulPhase.equals(transactionPhase))
        .count();
  }

  /**
   * @param request Transaction request metadata.
   */
  @Override
  public ImmutableList<PrivacyBudgetUnit> execute(TransactionRequest request)
      throws TransactionEngineException {
    Transaction transaction = initializeTransaction(request);
    proceedToNextPhase(TransactionPhase.NOTSTARTED, transaction);
    return transaction.getExhaustedPrivacyBudgetUnits();
  }

  /**
   * @param transactionPhaseRequest Transaction phase request metadata.
   */
  @Override
  public void executePhase(TransactionPhaseRequest transactionPhaseRequest)
      throws TransactionEngineException {}
}
