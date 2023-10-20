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

package com.google.scp.coordinator.privacy.budgeting.service.common.validation;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static java.time.temporal.ChronoUnit.DAYS;

import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import java.time.Instant;

/** Contains common operations for validating request inputs for the privacy budgeting service. */
public final class PrivacyBudgetingServiceValidations {

  private PrivacyBudgetingServiceValidations() {}

  private static boolean isWithinRange(Instant min, Instant max, Instant toCheck) {
    return toCheck.isAfter(min) && toCheck.isBefore(max);
  }

  /**
   * Checks whether any privacy budget units in the list have reporting windows outside of the
   * accepted time range. The minimum valid reporting window is controlled by the
   * reportingWindowOffsetInDays variable. THe maximum valid report time is the current time.
   *
   * @param privacyBudgetUnits List of privacy budget units to validate.
   * @param reportingWindowOffsetInDays The offset in which a privacy budget unit is still
   *     considered valid. The minimum time is considered to be now() - offset (in days).
   * @return A list containing the invalid reports. Empty list if provided if all are valid.
   */
  public static ImmutableList<PrivacyBudgetUnit> checkForInvalidReportingWindows(
      ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits, int reportingWindowOffsetInDays) {
    Instant now = Instant.now();
    Instant min = now.minus(reportingWindowOffsetInDays, DAYS);

    return privacyBudgetUnits.stream()
        .filter(privacyBudgetUnit -> !isWithinRange(min, now, privacyBudgetUnit.reportingWindow()))
        .collect(toImmutableList());
  }
}
