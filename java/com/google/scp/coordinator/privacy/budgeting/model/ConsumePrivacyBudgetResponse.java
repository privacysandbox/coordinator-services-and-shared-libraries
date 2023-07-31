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
import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.databind.annotation.JsonDeserialize;
import com.fasterxml.jackson.databind.annotation.JsonSerialize;
import com.google.auto.value.AutoValue;
import com.google.common.collect.ImmutableList;

/** Response Object for consuming privacy budget. */
@AutoValue
@JsonDeserialize(builder = ConsumePrivacyBudgetResponse.Builder.class)
@JsonSerialize(as = ConsumePrivacyBudgetResponse.class)
@JsonIgnoreProperties(ignoreUnknown = true)
public abstract class ConsumePrivacyBudgetResponse {

  public static final ConsumePrivacyBudgetResponse BUDGET_AVAILABLE_FOR_ALL =
      ConsumePrivacyBudgetResponse.builder()
          .exhaustedPrivacyBudgetUnits(ImmutableList.of())
          .build();

  public static ConsumePrivacyBudgetResponse.Builder builder() {
    return ConsumePrivacyBudgetResponse.Builder.builder();
  }

  /** Creates a Builder with values copied over. Used for creating an updated object. */
  public abstract ConsumePrivacyBudgetResponse.Builder toBuilder();

  /** Exhausted privacy budget units */
  @JsonProperty("exhausted_privacy_budget_units")
  public abstract ImmutableList<PrivacyBudgetUnit> exhaustedPrivacyBudgetUnits();

  @JsonIgnoreProperties(ignoreUnknown = true)
  @AutoValue.Builder
  public abstract static class Builder {

    @JsonCreator
    public static ConsumePrivacyBudgetResponse.Builder builder() {
      return new AutoValue_ConsumePrivacyBudgetResponse.Builder();
    }

    @JsonProperty("exhausted_privacy_budget_units")
    public abstract Builder exhaustedPrivacyBudgetUnits(
        ImmutableList<PrivacyBudgetUnit> exhaustedPrivacyBudgetUnits);

    public abstract ConsumePrivacyBudgetResponse build();
  }
}
