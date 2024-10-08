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
import com.google.inject.Inject;
import com.google.scp.coordinator.privacy.budgeting.model.ReportingOriginToPrivacyBudgetUnits;
import java.util.ArrayList;
import java.util.List;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class TransactionOrchestratorImpl implements TransactionOrchestrator {

  private static final Logger logger = LoggerFactory.getLogger(TransactionOrchestratorImpl.class);
  private final ImmutableList<PrivacyBudgetClientV2> privacyBudgetClients;

  /** Creates a new instance of the {@code TransactionOrchestratorImpl} class. */
  @Inject
  public TransactionOrchestratorImpl(ImmutableList<PrivacyBudgetClientV2> privacyBudgetClients) {
    this.privacyBudgetClients = privacyBudgetClients;
  }

  /**
   * Calls HEALTH-CHECK on a coordinator.
   *
   * @param privacyBudgetClient The client to use for the health check.
   * @param transactionRequest The transaction request data.
   * @throws TransactionOrchestratorException If the health check fails.
   */
  private void performHealthCheck(
      PrivacyBudgetClientV2 privacyBudgetClient, TransactionRequest transactionRequest)
      throws TransactionOrchestratorException {
    String privacyBudgetServerIdentifier = privacyBudgetClient.getPrivacyBudgetServerIdentifier();
    logger.info(
        "[{}] Executing phase '{}' against coordinator: '{}'",
        transactionRequest.transactionId(),
        TransactionPhase.HEALTH_CHECK,
        privacyBudgetServerIdentifier);

    HealthCheckResponse healthCheckResponse =
        privacyBudgetClient.performActionHealthCheck(transactionRequest);
    logger.info(
        "[{}] Phase '{}' finished against coordinator: '{}' with execution result: '{}'",
        transactionRequest.transactionId(),
        TransactionPhase.HEALTH_CHECK,
        privacyBudgetServerIdentifier,
        healthCheckResponse.executionResult());
    if (healthCheckResponse.executionResult().executionStatus() == ExecutionStatus.SUCCESS) {
      return;
    }

    logger.error(
        "[{}] Phase '{}' against coordinator: '{}' has failed.",
        transactionRequest.transactionId(),
        TransactionPhase.HEALTH_CHECK,
        privacyBudgetServerIdentifier);
    throw new TransactionOrchestratorException(
        StatusCode.TRANSACTION_ORCHESTRATOR_HEALTH_CHECK_FAILURE);
  }

  /**
   * Calls CONSUME-BUDGET on a coordinator.
   *
   * @param privacyBudgetClient The client to use for the health check.
   * @param transactionRequest The transaction request data.
   * @throws TransactionOrchestratorException If consume budget fails.
   */
  private ConsumeBudgetResponse performConsumeBudget(
      PrivacyBudgetClientV2 privacyBudgetClient, TransactionRequest transactionRequest)
      throws TransactionOrchestratorException {
    String privacyBudgetServerIdentifier = privacyBudgetClient.getPrivacyBudgetServerIdentifier();
    logger.info(
        "[{}] Executing phase '{}' against coordinator: '{}'",
        transactionRequest.transactionId(),
        TransactionPhase.CONSUME_BUDGET,
        privacyBudgetServerIdentifier);

    ConsumeBudgetResponse consumeBudgetResponse =
        privacyBudgetClient.performActionConsumeBudget(transactionRequest);
    logger.info(
        "[{}] Phase '{}' finished against coordinator: '{}' with execution result: '{}'",
        transactionRequest.transactionId(),
        TransactionPhase.CONSUME_BUDGET,
        privacyBudgetServerIdentifier,
        consumeBudgetResponse.executionResult());
    // Return the response if consuming budget was successful or if the budget was exhausted.
    if (consumeBudgetResponse.executionResult().executionStatus() == ExecutionStatus.SUCCESS
        || consumeBudgetResponse.executionResult().statusCode()
            == StatusCode.PRIVACY_BUDGET_CLIENT_BUDGET_EXHAUSTED) {
      return consumeBudgetResponse;
    }

    logger.error(
        "[{}] Phase '{}' against coordinator: '{}' has failed.",
        transactionRequest.transactionId(),
        TransactionPhase.CONSUME_BUDGET,
        privacyBudgetServerIdentifier);
    // It is possible for the coordinators to go out of sync at this point (e.g., if we consume
    // budget in one coordinator and then something goes wrong in the second). We do not throw an
    // exception in the case that PBS returns budget exhausted because that is expected behavior
    // when trying to consume budget that has already been consumed.
    throw new TransactionOrchestratorException(
        StatusCode.TRANSACTION_ORCHESTRATOR_CONSUME_BUDGET_FAILURE);
  }

  @Override
  public ImmutableList<ReportingOriginToPrivacyBudgetUnits> execute(
      TransactionRequest transactionRequest) throws TransactionOrchestratorException {
    for (PrivacyBudgetClientV2 privacyBudgetClient : this.privacyBudgetClients) {
      performHealthCheck(privacyBudgetClient, transactionRequest);
    }

    List<ConsumeBudgetResponse> consumeBudgetResponses = new ArrayList<>();
    // We loop through all clients before returning to ensure we consume budget in all coordinators.
    for (PrivacyBudgetClientV2 privacyBudgetClient : this.privacyBudgetClients) {
      consumeBudgetResponses.add(performConsumeBudget(privacyBudgetClient, transactionRequest));
    }

    // It is possible for each coordinator to have a different response. With that in mind, if any
    // of these coordinators do not allow for budget consumption, we return the list of exhausted
    // privacy budget units from the first coordinator with exhausted budget.
    for (ConsumeBudgetResponse consumeBudgetResponse : consumeBudgetResponses) {
      if (!consumeBudgetResponse.exhaustedPrivacyBudgetUnitsByOrigin().isEmpty()) {
        return consumeBudgetResponse.exhaustedPrivacyBudgetUnitsByOrigin();
      }
    }

    return ImmutableList.of();
  }
}
