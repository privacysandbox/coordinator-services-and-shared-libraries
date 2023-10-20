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

package com.google.scp.coordinator.privacy.budgeting.dao.aws.model.converter;

import com.google.common.base.Converter;
import com.google.scp.coordinator.privacy.budgeting.dao.aws.model.AwsPrivacyBudgetRecord;
import com.google.scp.coordinator.privacy.budgeting.dao.model.PrivacyBudgetRecord;

/** Converts between {@link PrivacyBudgetRecord} and {@link AwsPrivacyBudgetRecord} */
public final class AwsPrivacyBudgetConverter
    extends Converter<PrivacyBudgetRecord, AwsPrivacyBudgetRecord> {

  @Override
  protected AwsPrivacyBudgetRecord doForward(PrivacyBudgetRecord privacyBudget) {
    return AwsPrivacyBudgetRecord.builder()
        .privacyBudgetRecordKey(privacyBudget.privacyBudgetKey())
        .reportingWindow(privacyBudget.reportingWindow())
        .consumedBudgetCount(privacyBudget.consumedBudgetCount())
        .build();
  }

  @Override
  protected PrivacyBudgetRecord doBackward(AwsPrivacyBudgetRecord awsPrivacyBudget) {
    return PrivacyBudgetRecord.builder()
        .privacyBudgetKey(awsPrivacyBudget.privacyBudgetRecordKey())
        .reportingWindow(awsPrivacyBudget.reportingWindow())
        .consumedBudgetCount(awsPrivacyBudget.consumedBudgetCount())
        .build();
  }
}
