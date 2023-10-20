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

package com.google.scp.operator.cpio.distributedprivacybudgetclient;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.databind.annotation.JsonDeserialize;
import com.google.auto.value.AutoValue;
import com.google.common.collect.ImmutableList;

/**
 * Response object for the response data returned by PrivacyBudgetService's prepare API in scenarios
 * where some keys have their privacy budget exhausted.
 */
@AutoValue
@JsonDeserialize(builder = BudgetExhaustedResponse.Builder.class)
abstract class BudgetExhaustedResponse {
  /** Indices of the privacy budget keys from the request which have their budget exhausted. */
  @JsonProperty("f")
  abstract ImmutableList<Integer> budgetExhaustedIndices();

  @JsonProperty("v")
  abstract String version();

  @JsonIgnoreProperties(ignoreUnknown = true)
  @AutoValue.Builder
  public abstract static class Builder {

    @JsonCreator
    public static BudgetExhaustedResponse.Builder builder() {
      return new AutoValue_BudgetExhaustedResponse.Builder();
    }

    @JsonProperty("f")
    public abstract BudgetExhaustedResponse.Builder budgetExhaustedIndices(
        ImmutableList<Integer> budgetExhaustedIndices);

    @JsonProperty("v")
    public abstract BudgetExhaustedResponse.Builder version(String version);

    public abstract BudgetExhaustedResponse build();
  }
}
