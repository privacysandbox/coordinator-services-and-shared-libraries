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

/** Response object for the response data returned by PrivacyBudgetService's status API. */
@AutoValue
@JsonDeserialize(builder = TransactionStatusResponse.Builder.class)
abstract class TransactionStatusResponse {
  @JsonProperty("is_expired")
  abstract boolean isExpired();

  @JsonProperty("has_failures")
  abstract boolean hasFailed();

  @JsonProperty("transaction_execution_phase")
  abstract TransactionPhase currentPhase();

  @JsonProperty("last_execution_timestamp")
  abstract String lastExecutionTimestamp();

  @JsonIgnoreProperties(ignoreUnknown = true)
  @AutoValue.Builder
  public abstract static class Builder {

    @JsonCreator
    public static TransactionStatusResponse.Builder builder() {
      return new AutoValue_TransactionStatusResponse.Builder()
          .isExpired(false)
          .hasFailed(false)
          .lastExecutionTimestamp("")
          .currentPhase(TransactionPhase.UNKNOWN);
    }

    @JsonProperty("is_expired")
    public abstract TransactionStatusResponse.Builder isExpired(boolean isExpired);

    @JsonProperty("has_failures")
    public abstract TransactionStatusResponse.Builder hasFailed(boolean hasFailures);

    @JsonProperty("last_execution_timestamp")
    public abstract TransactionStatusResponse.Builder lastExecutionTimestamp(
        String lastExecutionTimestamp);

    @JsonProperty("transaction_execution_phase")
    public abstract TransactionStatusResponse.Builder currentPhase(TransactionPhase executionPhase);

    public abstract TransactionStatusResponse build();
  }
}
