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

package com.google.scp.coordinator.privacy.budgeting.dao.model.converter;

import com.google.common.base.Converter;
import com.google.scp.coordinator.privacy.budgeting.dao.model.PrivacyBudgetRecord;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;

/** Converts between {@link PrivacyBudgetUnit} and {@link PrivacyBudgetRecord} */
public final class PrivacyBudgetUnitConverter
    extends Converter<PrivacyBudgetRecord, PrivacyBudgetUnit> {

  @Override
  protected PrivacyBudgetUnit doForward(PrivacyBudgetRecord privacyBudgetRecord) {
    return PrivacyBudgetUnit.builder()
        .privacyBudgetKey(privacyBudgetRecord.privacyBudgetKey())
        .reportingWindow(privacyBudgetRecord.reportingWindow())
        .build();
  }

  @Override
  protected PrivacyBudgetRecord doBackward(PrivacyBudgetUnit privacyBudgetUnit) {
    return PrivacyBudgetRecord.builder()
        .privacyBudgetKey(privacyBudgetUnit.privacyBudgetKey())
        .reportingWindow(privacyBudgetUnit.reportingWindow())
        .consumedBudgetCount(0) // The key does not contain the consumed budget count.
        .build();
  }
}
