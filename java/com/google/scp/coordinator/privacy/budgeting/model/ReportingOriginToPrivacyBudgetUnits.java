/*
 * Copyright 2024 Google LLC
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

package com.google.scp.coordinator.privacy.budgeting.model;

import com.google.auto.value.AutoValue;
import com.google.common.collect.ImmutableList;
import com.google.errorprone.annotations.Immutable;

/**
 * Class encapsulating a pair of Reporting Origin and a list of {@link PrivacyBudgetUnit} for that
 * specific Reporting Origin
 */
@Immutable
@AutoValue
public abstract class ReportingOriginToPrivacyBudgetUnits {
  public static ReportingOriginToPrivacyBudgetUnits.Builder builder() {
    return ReportingOriginToPrivacyBudgetUnits.Builder.builder();
  }

  @AutoValue.Builder
  public abstract static class Builder {
    public static ReportingOriginToPrivacyBudgetUnits.Builder builder() {
      return new AutoValue_ReportingOriginToPrivacyBudgetUnits.Builder();
    }

    public abstract ReportingOriginToPrivacyBudgetUnits.Builder setReportingOrigin(
        String reportingOrigin);

    public abstract ReportingOriginToPrivacyBudgetUnits.Builder setPrivacyBudgetUnits(
        ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits);

    public abstract ReportingOriginToPrivacyBudgetUnits build();
  }

  public abstract String reportingOrigin();

  public abstract ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits();
}
