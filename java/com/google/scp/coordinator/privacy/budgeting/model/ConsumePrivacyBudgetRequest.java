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

package com.google.scp.coordinator.privacy.budgeting.model;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.fasterxml.jackson.annotation.JsonInclude;
import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.databind.annotation.JsonDeserialize;
import com.fasterxml.jackson.databind.annotation.JsonSerialize;
import com.google.auto.value.AutoValue;
import com.google.common.collect.ImmutableList;
import java.util.Optional;

/** Request object for consuming privacy budget. */
@AutoValue
@JsonDeserialize(builder = ConsumePrivacyBudgetRequest.Builder.class)
@JsonSerialize(as = ConsumePrivacyBudgetRequest.class)
@JsonIgnoreProperties(ignoreUnknown = true)
public abstract class ConsumePrivacyBudgetRequest {
  public static ConsumePrivacyBudgetRequest.Builder builder() {
    return ConsumePrivacyBudgetRequest.Builder.builder();
  }

  /** Creates a Builder with values copied over. Used for creating an updated object. */
  public abstract ConsumePrivacyBudgetRequest.Builder toBuilder();

  /** ad-tech origin where reports will be sent */
  @JsonProperty("attribution_report_to")
  public abstract String attributionReportTo();

  /**
   * Optional field for advertisers to set a higher limit for privacy budgets. When this field is
   * set, it overrides the default privacy budget limit that each privacy budget unit is allowed to
   * consume.
   */
  @JsonProperty("privacy_budget_limit")
  @JsonInclude(JsonInclude.Include.NON_ABSENT)
  public abstract Optional<Integer> privacyBudgetLimit();

  /** List of Privacy budgeting units */
  @JsonProperty("privacy_budget_units")
  public abstract ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits();

  @JsonIgnoreProperties(ignoreUnknown = true)
  @AutoValue.Builder
  public abstract static class Builder {

    @JsonCreator
    public static ConsumePrivacyBudgetRequest.Builder builder() {
      return new AutoValue_ConsumePrivacyBudgetRequest.Builder();
    }

    @JsonProperty("attribution_report_to")
    public abstract Builder attributionReportTo(String attributionReportTo);

    @JsonProperty("privacy_budget_units")
    public abstract Builder privacyBudgetUnits(ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits);

    @JsonProperty("privacy_budget_limit")
    public abstract Builder privacyBudgetLimit(Integer privacyBudgetLimit);

    public abstract ConsumePrivacyBudgetRequest build();
  }
}
