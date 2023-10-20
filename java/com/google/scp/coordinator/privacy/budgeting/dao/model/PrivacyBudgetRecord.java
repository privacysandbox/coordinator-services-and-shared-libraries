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

package com.google.scp.coordinator.privacy.budgeting.dao.model;

import com.google.auto.value.AutoValue;
import java.time.Instant;

/**
 * A model for the object that is stored in the privacy budget database. This is a generic model
 * that is not provider-specific.
 */
@AutoValue
public abstract class PrivacyBudgetRecord {

  public static Builder builder() {
    return new AutoValue_PrivacyBudgetRecord.Builder();
  }

  public abstract PrivacyBudgetRecord.Builder toBuilder();

  public abstract String privacyBudgetKey();

  public abstract Instant reportingWindow();

  public abstract Integer consumedBudgetCount();

  @AutoValue.Builder
  public abstract static class Builder {

    public abstract Builder privacyBudgetKey(String privacyBudgetKey);

    public abstract Builder reportingWindow(Instant reportingWindow);

    public abstract Builder consumedBudgetCount(Integer consumedBudgetCount);

    public abstract PrivacyBudgetRecord build();
  }
}
