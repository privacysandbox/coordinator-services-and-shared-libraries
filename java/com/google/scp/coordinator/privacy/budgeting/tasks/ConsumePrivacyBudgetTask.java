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

package com.google.scp.coordinator.privacy.budgeting.tasks;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.scp.coordinator.privacy.budgeting.service.common.validation.PrivacyBudgetingServiceValidations.checkForInvalidReportingWindows;

import com.google.common.base.Strings;
import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.inject.Singleton;
import com.google.scp.coordinator.privacy.budgeting.dao.common.PrivacyBudgetDatabaseBridge;
import com.google.scp.coordinator.privacy.budgeting.dao.model.PrivacyBudgetRecord;
import com.google.scp.coordinator.privacy.budgeting.dao.model.converter.PrivacyBudgetUnitConverter;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.PrivacyBudgetDatabase;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.PrivacyBudgetLimit;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.ReportingWindowOffset;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.exception.SharedErrorReason;
import com.google.scp.shared.api.model.Code;

/** Service class that handles the business logic for ConsumePrivacyBudget API. */
@Singleton
public final class ConsumePrivacyBudgetTask {

  private final PrivacyBudgetDatabaseBridge privacyBudgetDatabase;
  private final int defaultPrivacyBudgetLimit;
  private final int reportingWindowOffset;
  private final PrivacyBudgetUnitConverter privacyBudgetUnitConverter;

  @Inject
  ConsumePrivacyBudgetTask(
      @PrivacyBudgetDatabase PrivacyBudgetDatabaseBridge privacyBudgetDatabase,
      @PrivacyBudgetLimit int defaultPrivacyBudgetLimit,
      @ReportingWindowOffset int reportingWindowOffset,
      PrivacyBudgetUnitConverter privacyBudgetUnitConverter) {
    this.privacyBudgetDatabase = privacyBudgetDatabase;
    this.defaultPrivacyBudgetLimit = defaultPrivacyBudgetLimit;
    this.reportingWindowOffset = reportingWindowOffset;
    this.privacyBudgetUnitConverter = privacyBudgetUnitConverter;
  }

  /** Performs validation on the incoming request */
  private void validateRequest(ConsumePrivacyBudgetRequest consumePrivacyBudgetRequest)
      throws ServiceException {
    if (Strings.isNullOrEmpty(consumePrivacyBudgetRequest.attributionReportTo())) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          SharedErrorReason.INVALID_ARGUMENT.toString(),
          "Attribution report to cannot be empty.");
    }

    if (consumePrivacyBudgetRequest.privacyBudgetUnits().isEmpty()) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          SharedErrorReason.INVALID_ARGUMENT.toString(),
          "privacy_budget_units was empty. A non-empty list must be provided.");
    }

    ImmutableList<PrivacyBudgetUnit> privacyBudgetUnitsWithEmptyKeys =
        consumePrivacyBudgetRequest.privacyBudgetUnits().stream()
            .filter(key -> key.privacyBudgetKey().isEmpty())
            .collect(toImmutableList());
    if (!privacyBudgetUnitsWithEmptyKeys.isEmpty()) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          SharedErrorReason.INVALID_ARGUMENT.toString(),
          String.format(
              "The value of privacy_budget_key cannot be empty. The following privacy budget units"
                  + " had an empty privacy_budget_key: %s",
              privacyBudgetUnitsWithEmptyKeys));
    }

    ImmutableList<PrivacyBudgetUnit> privacyBudgetUnitsWithInvalidWindows =
        checkForInvalidReportingWindows(
            consumePrivacyBudgetRequest.privacyBudgetUnits(), reportingWindowOffset);
    if (!privacyBudgetUnitsWithInvalidWindows.isEmpty()) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          SharedErrorReason.INVALID_ARGUMENT.toString(),
          String.format(
              "Reporting windows need to be within the last %d days. The following privacy budget"
                  + " units had invalid reporting_window values: %s",
              reportingWindowOffset, privacyBudgetUnitsWithInvalidWindows));
    }

    if (consumePrivacyBudgetRequest.privacyBudgetLimit().isPresent()
        && consumePrivacyBudgetRequest.privacyBudgetLimit().get() <= 0) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          SharedErrorReason.INVALID_ARGUMENT.toString(),
          "privacy_budget_limit must be greater than 0.");
    }
  }

  private ConsumePrivacyBudgetResponse getExhaustedResponse(
      ImmutableList<PrivacyBudgetUnit> exhaustedPrivacyBudgetUnits) {
    return ConsumePrivacyBudgetResponse.builder()
        .exhaustedPrivacyBudgetUnits(exhaustedPrivacyBudgetUnits)
        .build();
  }

  /**
   * Carries out business logic to handle the consume request.
   *
   * <p>1. Validate the request.
   *
   * <p>2. Check the state of the DB: If all available continue, if some are exhausted return early
   * with a response of the budgets not available.
   *
   * <p>3. Consume for units in the request. If some are now not available (due to concurrent
   * consume requests) don't change the response, over allowing requests is ok in this scenario.
   *
   * <p>4. Send the success response.
   *
   * @param consumePrivacyBudgetRequest the request to consume budget
   * @return the response object to be sent back to the requestor
   * @throws ServiceException for any bad input errors or for any system errors
   */
  public ConsumePrivacyBudgetResponse handleRequest(
      ConsumePrivacyBudgetRequest consumePrivacyBudgetRequest) throws ServiceException {

    // 1. Ensure that the request is valid
    validateRequest(consumePrivacyBudgetRequest);

    // 2. Retrieve current state from DB
    ImmutableList<PrivacyBudgetUnit> privacyBudgetUnitsFromRequest =
        consumePrivacyBudgetRequest.privacyBudgetUnits();
    ImmutableList<PrivacyBudgetRecord> privacyBudgetsFromDb =
        getPrivacyBudgets(
            privacyBudgetUnitsFromRequest, consumePrivacyBudgetRequest.attributionReportTo());

    // 2. Check if any budgets are exhausted. Use privacy_budget_limit if specified in the request,
    // otherwise use the default value.
    int limitFromRequestOrDefault = getLimitFromRequestOrDefault(consumePrivacyBudgetRequest);
    ImmutableList<PrivacyBudgetUnit> exhaustedPrivacyBudgetUnits =
        getExhaustedPrivacyBudgetUnits(privacyBudgetsFromDb, limitFromRequestOrDefault);

    // 2. Return early and don't consume if any units are at the limit
    if (!exhaustedPrivacyBudgetUnits.isEmpty()) {
      return getExhaustedResponse(exhaustedPrivacyBudgetUnits);
    }

    // 3. Consume for units in the request, don't change the response if some budgets are not
    // available.
    consumePrivacyBudgets(
        privacyBudgetUnitsFromRequest, consumePrivacyBudgetRequest.attributionReportTo());

    // 4. Return the success response
    return ConsumePrivacyBudgetResponse.BUDGET_AVAILABLE_FOR_ALL;
  }

  private int getLimitFromRequestOrDefault(ConsumePrivacyBudgetRequest request) {
    return request.privacyBudgetLimit().orElse(defaultPrivacyBudgetLimit);
  }

  private ImmutableList<PrivacyBudgetRecord> getPrivacyBudgets(
      ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits, String attributionReportTo) {
    return privacyBudgetDatabase.getPrivacyBudgets(privacyBudgetUnits, attributionReportTo);
  }

  /** Returns the privacy budget units which are at or above the consumption limit in the DB */
  private ImmutableList<PrivacyBudgetUnit> getExhaustedPrivacyBudgetUnits(
      ImmutableList<PrivacyBudgetRecord> recordsFromDatabase, int privacyBudgetLimit) {
    return recordsFromDatabase.stream()
        .filter(
            privacyBudgetRecord -> privacyBudgetRecord.consumedBudgetCount() >= privacyBudgetLimit)
        .map(privacyBudgetUnitConverter::convert)
        .collect(toImmutableList());
  }

  private ImmutableList<PrivacyBudgetRecord> consumePrivacyBudgets(
      ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits, String attributionReportTo) {
    return privacyBudgetDatabase.incrementPrivacyBudgets(privacyBudgetUnits, attributionReportTo);
  }
}
