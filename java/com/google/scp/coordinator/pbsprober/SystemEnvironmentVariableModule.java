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

import com.google.common.base.Strings;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.coordinator.pbsprober.Annotations.ReportingOrigin;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClientModule.CoordinatorAPrivacyBudgetServiceAuthEndpoint;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClientModule.CoordinatorAPrivacyBudgetServiceBaseUrl;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClientModule.CoordinatorBPrivacyBudgetServiceAuthEndpoint;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClientModule.CoordinatorBPrivacyBudgetServiceBaseUrl;

/** Module to provide environment variable using System.getEnv */
public final class SystemEnvironmentVariableModule extends AbstractModule {
  private static final String COORDINATOR_A_ENDPOINT = "coordinator_a_privacy_budgeting_endpoint";
  private static final String COORDINATOR_A_AUTH_ENDPOINT =
      "coordinator_a_privacy_budget_service_auth_endpoint";
  private static final String COORDINATOR_B_ENDPOINT = "coordinator_b_privacy_budgeting_endpoint";
  private static final String COORDINATOR_B_AUTH_ENDPOINT =
      "coordinator_b_privacy_budget_service_auth_endpoint";
  private static final String REPORTING_ORIGIN = "reporting_origin";

  @Provides
  @CoordinatorAPrivacyBudgetServiceBaseUrl
  String provideCoordinatorAPrivacyBudgetingEndpoint() {
    return getEnvironmentVariable(COORDINATOR_A_ENDPOINT);
  }

  @Provides
  @CoordinatorAPrivacyBudgetServiceAuthEndpoint
  String provideCoordinatorAPrivacyBudgetServiceAuthEndpoint() {
    return getEnvironmentVariable(COORDINATOR_A_AUTH_ENDPOINT);
  }

  @Provides
  @CoordinatorBPrivacyBudgetServiceBaseUrl
  String provideCoordinatorBPrivacyBudgetingEndpoint() {
    return getEnvironmentVariable(COORDINATOR_B_ENDPOINT);
  }

  @Provides
  @CoordinatorBPrivacyBudgetServiceAuthEndpoint
  String provideCoordinatorBPrivacyBudgetServiceAuthEndpoint() {
    return getEnvironmentVariable(COORDINATOR_B_AUTH_ENDPOINT);
  }

  @Provides
  @ReportingOrigin
  String provideReportingOrigin() {
    return getEnvironmentVariable(REPORTING_ORIGIN);
  }

  private static String getEnvironmentVariable(String key) {
    String result = System.getenv(key);
    if (Strings.isNullOrEmpty(result)) {
      throw new RuntimeException(key + " must be set");
    }
    return result;
  }
}
