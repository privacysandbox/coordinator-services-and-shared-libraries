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

package com.google.scp.operator.cpio.distributedprivacybudgetclient.testing;

import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClient;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.StatusCode;
import java.time.Instant;

/**
 * Fake distributed privacy budgeting bridge to be used strictly in tests
 *
 * <p>NOTE: This implementation is **NOT** thread-safe.
 *
 * <p>This bridge supports programmatically setting the budget.
 */
public final class FakeDistributedPrivacyBudgetClient implements DistributedPrivacyBudgetClient {

  private boolean shouldThrow;
  private boolean shouldReturnExhausted;
  private ConsumePrivacyBudgetRequest lastRequest;

  /** Creates a new instance of the {@code FakePrivacyBudgetClient} class. */
  public FakeDistributedPrivacyBudgetClient() {
    shouldThrow = false;
    shouldReturnExhausted = false;
  }

  public void setShouldThrow() {
    shouldThrow = true;
  }

  public void setShouldReturnExhausted() {
    shouldReturnExhausted = true;
  }

  @Override
  public ConsumePrivacyBudgetResponse consumePrivacyBudget(
      ConsumePrivacyBudgetRequest privacyBudgetRequest)
      throws DistributedPrivacyBudgetServiceException {
    if (shouldThrow) {
      throw new DistributedPrivacyBudgetServiceException(
          StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHENTICATED,
          new IllegalStateException("Set to throw"));
    }
    lastRequest = privacyBudgetRequest;
    if (shouldReturnExhausted) {
      return ConsumePrivacyBudgetResponse.builder()
          .exhaustedPrivacyBudgetUnits(
              ImmutableList.of(
                  PrivacyBudgetUnit.builder()
                      .privacyBudgetKey("key")
                      .reportingWindow(Instant.EPOCH)
                      .build()))
          .build();
    }
    return ConsumePrivacyBudgetResponse.builder()
        .exhaustedPrivacyBudgetUnits(ImmutableList.of())
        .build();
  }

  public ConsumePrivacyBudgetRequest getLastRequest() {
    return lastRequest;
  }
}
