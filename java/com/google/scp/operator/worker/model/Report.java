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

package com.google.scp.operator.worker.model;

import com.google.auto.value.AutoValue;
import com.google.common.collect.ImmutableList;
import com.google.scp.privacy.budgeting.model.PrivacyBudgetKey;
import java.time.Instant;

/**
 * Representation of a single report
 *
 * <p>Contains a key for tracking privacy budgets, list of aggregation facts, and other data for
 * request handling.
 */
@AutoValue
public abstract class Report {

  public static Builder builder() {
    return new AutoValue_Report.Builder();
  }

  public abstract PrivacyBudgetKey privacyBudgetKey();

  public abstract ImmutableList<Fact> facts();

  public abstract Instant originalReportTime();

  // Intended eTLD+1 destination of the ad click
  public abstract String attributionDestination();

  // Endpoint where the attribution report will be sent
  public abstract String attributionReportTo();

  @AutoValue.Builder
  public abstract static class Builder {

    public abstract Builder setPrivacyBudgetKey(PrivacyBudgetKey privacyBudgetKey);

    abstract ImmutableList.Builder<Fact> factsBuilder();

    public Builder addFact(Fact fact) {
      factsBuilder().add(fact);
      return this;
    }

    public Builder addAllFact(Iterable<Fact> facts) {
      factsBuilder().addAll(facts);
      return this;
    }

    public abstract Builder setOriginalReportTime(Instant originalReportTime);

    public abstract Builder setAttributionDestination(String attributionDestination);

    public abstract Builder setAttributionReportTo(String attributionReportTo);

    public abstract Report build();
  }
}
