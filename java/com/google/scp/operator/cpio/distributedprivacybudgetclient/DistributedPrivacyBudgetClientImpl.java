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
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;
import com.google.scp.coordinator.privacy.budgeting.model.ReportingOriginToPrivacyBudgetUnits;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionManager.TransactionManagerException;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionOrchestrator.TransactionOrchestratorException;
import java.sql.Timestamp;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.UUID;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Client which runs Transaction manager and executes a transaction to consume privacy budget. */
public final class DistributedPrivacyBudgetClientImpl implements DistributedPrivacyBudgetClient {
  private static final Logger logger =
      LoggerFactory.getLogger(DistributedPrivacyBudgetClientImpl.class);
  private static final int PRIVACY_BUDGET_MAX_ITEM = 30000;
  private static final int TRANSACTION_SECRET_LENGTH = 10;
  private static final int TRANSACTION_TIMEOUT_MINUTES = 5;
  private final TransactionManager transactionManager;
  private final TransactionOrchestrator transactionOrchestrator;

  /** Creates a new instance of the {@code DistributedPrivacyBudgetClientImpl} class. */
  @Inject
  public DistributedPrivacyBudgetClientImpl(
      TransactionManager transactionManager, TransactionOrchestrator transactionOrchestrator) {
    this.transactionManager = transactionManager;
    this.transactionOrchestrator = transactionOrchestrator;
  }

  @Override
  public ConsumePrivacyBudgetResponse consumePrivacyBudget(ConsumePrivacyBudgetRequest request)
      throws DistributedPrivacyBudgetClientException, DistributedPrivacyBudgetServiceException {
    var size = getTotalCountOfPrivacyBudgetUnits(request);
    if (size > PRIVACY_BUDGET_MAX_ITEM) {
      throw new DistributedPrivacyBudgetClientException(
          String.format("Too many items. Current: %d; Max: %d", size, PRIVACY_BUDGET_MAX_ITEM));
    }

    TransactionRequest transactionRequest = buildTransactionRequest(request);
    try {
      ImmutableList<ReportingOriginToPrivacyBudgetUnits> exhaustedBudgetUnitsByOrigin;
      if (request.enableNewPbsClient().orElse(false)) {
        logger.info(
            "Starting distributed privacy budget service for the request with new client: "
                + request);
        exhaustedBudgetUnitsByOrigin = transactionOrchestrator.execute(transactionRequest);
      } else {
        logger.info("Starting distributed privacy budget service for the request: " + request);
        exhaustedBudgetUnitsByOrigin = transactionManager.execute(transactionRequest);
      }
      logger.info("Successfully ran distributed privacy budget service.");
      return ConsumePrivacyBudgetResponse.builder()
          .exhaustedPrivacyBudgetUnitsByOrigin(exhaustedBudgetUnitsByOrigin)
          .build();
    } catch (TransactionManagerException e) {
      throw new DistributedPrivacyBudgetServiceException(e.getStatusCode(), e);
    } catch (TransactionOrchestratorException e) {
      throw new DistributedPrivacyBudgetServiceException(e.getStatusCode(), e);
    }
  }

  private static long getTotalCountOfPrivacyBudgetUnits(ConsumePrivacyBudgetRequest request) {
    return request.reportingOriginToPrivacyBudgetUnitsList().stream()
        .map(ReportingOriginToPrivacyBudgetUnits::privacyBudgetUnits)
        .flatMap(ImmutableList::stream)
        .count();
  }

  private TransactionRequest buildTransactionRequest(ConsumePrivacyBudgetRequest request) {
    Instant expiryTime = Instant.now();
    expiryTime = expiryTime.plus(TRANSACTION_TIMEOUT_MINUTES, ChronoUnit.MINUTES);
    TransactionRequest.Builder transactionRequestBuilder =
        TransactionRequest.builder()
            .setTransactionId(UUID.randomUUID())
            .setClaimedIdentity(request.claimedIdentity())
            .setTrustedServicesClientVersion(request.trustedServicesClientVersion())
            .setReportingOriginToPrivacyBudgetUnitsList(
                request.reportingOriginToPrivacyBudgetUnitsList())
            .setTimeout(Timestamp.from(expiryTime))
            .setTransactionSecret(generateSecret());

    request.privacyBudgetLimit().ifPresent(transactionRequestBuilder::setPrivacyBudgetLimit);
    return transactionRequestBuilder.build();
  }

  private String generateSecret() {
    String secret = UUID.randomUUID().toString().replace("-", "");
    return secret.substring(0, TRANSACTION_SECRET_LENGTH);
  }
}
