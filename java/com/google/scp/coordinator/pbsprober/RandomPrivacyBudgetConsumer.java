/*
 * Copyright 2023 Google LLC
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

package com.google.scp.coordinator.pbsprober;

import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.pbsprober.Annotations.ReportingOrigin;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClient;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClient.DistributedPrivacyBudgetClientException;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClient.DistributedPrivacyBudgetServiceException;
import java.time.Instant;
import java.util.UUID;
import javax.inject.Inject;

/**
 * Consumes a privacy budget for a random privacy budget key. This class is intended to be used by
 * cloud specific lambda/function for privacy budget prober handler to consume privacy budget
 * randomly.
 */
public final class RandomPrivacyBudgetConsumer {
  private static final int DEFAULT_PRIVACY_BUDGET_LIMIT = 1;

  private final DistributedPrivacyBudgetClient client;
  private final String reportingOrigin;

  @Inject
  public RandomPrivacyBudgetConsumer(
      DistributedPrivacyBudgetClient client, @ReportingOrigin String reportingOrigin) {
    this.client = client;
    this.reportingOrigin = reportingOrigin;
  }

  /** Handles the request from client by consuming budget for a random privacy budget key. */
  public ConsumePrivacyBudgetResponse consumeBudget()
      throws DistributedPrivacyBudgetClientException, DistributedPrivacyBudgetServiceException {
    PrivacyBudgetUnit privacyBudgetUnit =
        PrivacyBudgetUnit.builder()
            .privacyBudgetKey(String.valueOf(UUID.randomUUID()))
            .reportingWindow(Instant.now())
            .build();
    ConsumePrivacyBudgetRequest consumeRequest =
        ConsumePrivacyBudgetRequest.builder()
            .attributionReportTo(reportingOrigin)
            .privacyBudgetUnits(ImmutableList.of(privacyBudgetUnit))
            .privacyBudgetLimit(DEFAULT_PRIVACY_BUDGET_LIMIT)
            .build();
    return client.consumePrivacyBudget(consumeRequest);
  }
}
