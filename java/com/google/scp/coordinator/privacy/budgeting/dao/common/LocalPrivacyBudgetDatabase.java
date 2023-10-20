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
import java.util.Hashtable;
import java.util.stream.Collectors;

/** An in-memory version of the PrivacyBudgetDatabase * */
public class LocalPrivacyBudgetDatabase implements PrivacyBudgetDatabaseBridge {

  private Hashtable<String, PrivacyBudgetRecord> privacyBudgetRecordsHash = new Hashtable<>();

  @Override
  public ImmutableList<PrivacyBudgetRecord> getPrivacyBudgets(
      ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits, String attributionReportTo) {

    return ImmutableList.copyOf(
        privacyBudgetUnits.stream()
            .filter(unit -> privacyBudgetRecordsHash.containsKey(getKey(unit, attributionReportTo)))
            .map(unit -> privacyBudgetRecordsHash.get(getKey(unit, attributionReportTo)))
            .collect(Collectors.toUnmodifiableList()));
  }

  private String getKey(PrivacyBudgetUnit privacyBudgetUnit, String attributionReportTo) {
    return String.format(
        "%s_%s_%s",
        privacyBudgetUnit.privacyBudgetKey(),
        privacyBudgetUnit.reportingWindow(),
        attributionReportTo);
  }

  @Override
  public ImmutableList<PrivacyBudgetRecord> incrementPrivacyBudgets(
      ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits, String attributionReportTo) {
    return ImmutableList.copyOf(
        privacyBudgetUnits.stream()
            .map(
                unit -> {
                  String key = getKey(unit, attributionReportTo);
                  if (!privacyBudgetRecordsHash.containsKey(key)) {
                    privacyBudgetRecordsHash.put(
                        key,
                        PrivacyBudgetRecord.builder()
                            .privacyBudgetKey(unit.privacyBudgetKey())
                            .reportingWindow(unit.reportingWindow())
                            .consumedBudgetCount(0)
                            .build());
                  }
                  PrivacyBudgetRecord record = privacyBudgetRecordsHash.get(key);
                  privacyBudgetRecordsHash.put(
                      key,
                      record.toBuilder()
                          .consumedBudgetCount(record.consumedBudgetCount() + 1)
                          .build());
                  return privacyBudgetRecordsHash.get(key);
                })
            .collect(Collectors.toUnmodifiableList()));
  }
}
