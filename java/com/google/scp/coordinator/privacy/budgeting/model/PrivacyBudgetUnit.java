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
import java.time.Instant;

/** Smallest unit that privacy budgeting is tracked for */
@AutoValue
@JsonDeserialize(builder = PrivacyBudgetUnit.Builder.class)
@JsonSerialize(as = PrivacyBudgetUnit.class)
@JsonIgnoreProperties(ignoreUnknown = true)
public abstract class PrivacyBudgetUnit {

  public static PrivacyBudgetUnit.Builder builder() {
    return PrivacyBudgetUnit.Builder.builder();
  }

  /** Creates a Builder with values copied over. Used for creating an updated object. */
  public abstract PrivacyBudgetUnit.Builder toBuilder();

  @JsonProperty("privacy_budget_key")
  public abstract String privacyBudgetKey();

  @JsonProperty("reporting_window")
  public abstract Instant reportingWindow();

  @JsonIgnoreProperties(ignoreUnknown = true)
  @AutoValue.Builder
  public abstract static class Builder {

    @JsonCreator
    public static PrivacyBudgetUnit.Builder builder() {
      return new AutoValue_PrivacyBudgetUnit.Builder();
    }

    @JsonProperty("privacy_budget_key")
    public abstract Builder privacyBudgetKey(String privacyBudgetKey);

    @JsonProperty("reporting_window")
    public abstract Builder reportingWindow(Instant reportingWindow);

    public abstract PrivacyBudgetUnit build();
  }
}
