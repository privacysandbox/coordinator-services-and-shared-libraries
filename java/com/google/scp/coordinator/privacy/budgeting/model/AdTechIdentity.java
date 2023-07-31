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

import com.google.auto.value.AutoValue;

/** Authentication metadata for requests by ad-techs */
@AutoValue
public abstract class AdTechIdentity {

  public static Builder builder() {
    return new AutoValue_AdTechIdentity.Builder();
  }

  /** The IAM Role of the requester */
  public abstract String iamRole();

  /** The attribution-report-to of the requester */
  public abstract String attributionReportTo();

  @AutoValue.Builder
  public abstract static class Builder {

    public abstract Builder setIamRole(String value);

    public abstract Builder setAttributionReportTo(String value);

    public abstract AdTechIdentity build();
  }
}
