/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.privacy.budgeting.service.common;

import com.google.inject.AbstractModule;
import com.google.scp.coordinator.privacy.budgeting.dao.common.PrivacyBudgetDatabaseBridge;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.PrivacyBudgetDatabase;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.PrivacyBudgetLimit;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.ReportingWindowOffset;

/**
 * A bridging interface for injections to pick up the right module to use when instantiating the
 * privacy budgeting service.
 */
public abstract class PrivacyBudgetingServiceModule extends AbstractModule {

  private static final String PRIVACY_BUDGET_LIMIT_ENV_VAR = "PRIVACY_BUDGET_LIMIT";
  private static final String REPORTING_WINDOW_OFFSET_DAYS_ENV_VAR = "REPORTING_WINDOW_OFFSET_DAYS";
  private static final String PRIVACY_BUDGET_TTL_BUFFER_DAYS_ENV_VAR =
      "PRIVACY_BUDGET_TTL_BUFFER_DAYS";

  protected int privacyBudgetLimit() {
    return Integer.parseInt(System.getenv(PRIVACY_BUDGET_LIMIT_ENV_VAR));
  }

  protected int reportingWindowOffsetInDays() {
    return Integer.parseInt(System.getenv(REPORTING_WINDOW_OFFSET_DAYS_ENV_VAR));
  }

  protected int privacyBudgetTtlBuffer() {
    return Integer.parseInt(System.getenv(PRIVACY_BUDGET_TTL_BUFFER_DAYS_ENV_VAR));
  }

  protected abstract Class<? extends PrivacyBudgetDatabaseBridge>
      getPrivacyBudgetDatabaseImplementation();

  @Override
  protected void configure() {
    bind(PrivacyBudgetDatabaseBridge.class)
        .annotatedWith(PrivacyBudgetDatabase.class)
        .to(getPrivacyBudgetDatabaseImplementation());
    bind(Integer.class).annotatedWith(PrivacyBudgetLimit.class).toInstance(privacyBudgetLimit());
    bind(Integer.class)
        .annotatedWith(ReportingWindowOffset.class)
        .toInstance(reportingWindowOffsetInDays());
    configureModule();
  }

  /**
   * Arbitrary configurations that can be done by the implementing class to support dependencies
   * that are specific to that implementation.
   */
  protected void configureModule() {}
}
