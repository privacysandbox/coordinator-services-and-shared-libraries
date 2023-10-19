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

/**
 * Interface for representing a decryption or validation error to be provided in the aggregation
 * response error summary.
 *
 * <p>Subclasses can implement access methods for fine details.
 */
@AutoValue
public abstract class ErrorMessage {

  public static Builder builder() {
    return new AutoValue_ErrorMessage.Builder();
  }

  /** The category of the error message, used for identifying the type of an error message */
  public abstract String category();

  /**
   * Detailed diagnostic information related to the error that would be useful for debugging or
   * error logging. Not intended to be provided in aggregation response.
   */
  public abstract String detailedErrorMessage();

  @AutoValue.Builder
  public abstract static class Builder {

    public abstract Builder setCategory(String category);

    public abstract Builder setDetailedErrorMessage(String detailedErrorMessage);

    public abstract ErrorMessage build();
  }
}
