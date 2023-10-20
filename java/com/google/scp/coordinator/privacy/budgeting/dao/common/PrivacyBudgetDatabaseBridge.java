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

package com.google.scp.coordinator.privacy.budgeting.dao.common;

import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.privacy.budgeting.dao.model.PrivacyBudgetRecord;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;

/** Interface for communicating with privacy budget database. */
public interface PrivacyBudgetDatabaseBridge {

  /**
   * Returns the privacy budgets for a list of privacy budget units. The returned privacy budgets
   * will contain the consumed budget for those keys. Only privacy budgets that have a matching
   * entry in the database will be returned. If a privacy budget does not exist in the database, it
   * will not be returned.
   *
   * @param privacyBudgetUnits The privacy budget units to query the database for. The
   *     privacyBudgetKey and reportingWindow fields are required and must be non-empty.
   * @param attributionReportTo The attribution report to (i.e. ad-tech id) for this privacy budget
   *     key.
   * @return the privacy budgets along with their consumed budgets that exist in the database. If a
   *     given privacy budget key has not previously consumed any budget, it will not exist in the
   *     database and therefore not part of the return.
   */
  ImmutableList<PrivacyBudgetRecord> getPrivacyBudgets(
      ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits, String attributionReportTo);

  /**
   * Adds 1 to the count of the privacy budgets for each key.
   *
   * @param privacyBudgetUnits The privacy budget units to consume for.
   * @param attributionReportTo The attribution report to (i.e. ad-tech id) for this privacy budget
   *     key.
   * @return The updated privacy budgets that contain the new consumed privacy budget count as
   *     exists in the database. If the privacy budget failed to update, it will not be returned as
   *     part of the result.
   */
  ImmutableList<PrivacyBudgetRecord> incrementPrivacyBudgets(
      ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits, String attributionReportTo);
}
