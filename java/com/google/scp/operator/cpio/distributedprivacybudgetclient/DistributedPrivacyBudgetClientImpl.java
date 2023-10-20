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
import com.google.common.primitives.Ints;
import com.google.inject.Inject;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionManager.TransactionManagerException;
import java.sql.Timestamp;
import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.UUID;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Client which runs Transaction manager and executes a transaction to consume privacy budget. */
public final class DistributedPrivacyBudgetClientImpl implements DistributedPrivacyBudgetClient {
  private static final Logger logger =
      LoggerFactory.getLogger(DistributedPrivacyBudgetClientImpl.class);
  private static final int REQUEST_TIMEOUT_DURATION =
      Ints.checkedCast(Duration.ofMinutes(1).toMillis());
  private static final int PRIVACY_BUDGET_MAX_ITEM = 30000;
  private static final int TRANSACTION_SECRET_LENGTH = 10;
  private static final int TRANSACTION_TIMEOUT_MINUTES = 5;
  private final TransactionManager transactionManager;

  /** Creates a new instance of the {@code DistributedPrivacyBudgetClientImpl} class. */
  @Inject
  public DistributedPrivacyBudgetClientImpl(TransactionManager transactionManager) {
    this.transactionManager = transactionManager;
  }

  @Override
  public ConsumePrivacyBudgetResponse consumePrivacyBudget(ConsumePrivacyBudgetRequest request)
      throws DistributedPrivacyBudgetClientException, DistributedPrivacyBudgetServiceException {
    var size = request.privacyBudgetUnits().size();
    if (size > PRIVACY_BUDGET_MAX_ITEM) {
      throw new DistributedPrivacyBudgetClientException(
          String.format("Too many items. Current: %d; Max: %d", size, PRIVACY_BUDGET_MAX_ITEM));
    }

    TransactionRequest transactionRequest = buildTransactionRequest(request);
    try {
      logger.info("Starting distributed privacy budget service for the request: " + request);
      ImmutableList<PrivacyBudgetUnit> exhaustedPrivacyBudgetUnits =
          transactionManager.execute(transactionRequest);

      logger.info("Successfully ran distributed privacy budget service.");
      return ConsumePrivacyBudgetResponse.builder()
          .exhaustedPrivacyBudgetUnits(exhaustedPrivacyBudgetUnits)
          .build();
    } catch (TransactionManagerException e) {
      throw new DistributedPrivacyBudgetServiceException(e.getStatusCode(), e);
    }
  }

  private TransactionRequest buildTransactionRequest(ConsumePrivacyBudgetRequest request) {
    Instant expiryTime = Instant.now();
    expiryTime = expiryTime.plus(TRANSACTION_TIMEOUT_MINUTES, ChronoUnit.MINUTES);
    TransactionRequest.Builder transactionRequestBuilder =
        TransactionRequest.builder()
            .setTransactionId(UUID.randomUUID())
            .setAttributionReportTo(request.attributionReportTo())
            .setPrivacyBudgetUnits(request.privacyBudgetUnits())
            .setTimeout(Timestamp.from(expiryTime))
            .setTransactionSecret(generateSecret());

    request
        .privacyBudgetLimit()
        .ifPresent(limit -> transactionRequestBuilder.setPrivacyBudgetLimit(limit));
    return transactionRequestBuilder.build();
  }

  private String generateSecret() {
    String secret = UUID.randomUUID().toString().replace("-", "");
    return secret.substring(0, TRANSACTION_SECRET_LENGTH);
  }
}
