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

import static com.google.common.base.Preconditions.checkState;

import com.google.auto.value.AutoValue;
import com.google.common.collect.ImmutableList;
import java.util.Optional;

/**
 * Result from Record Decryption
 *
 * <p>Contains either a report that was decrypted successfully or a series of errors resulting from
 * decryption. Errors can later be aggregated and provided to requesters for diagnostic information.
 */
@AutoValue
public abstract class DecryptionResult {

  public static Builder builder() {
    return new AutoValue_DecryptionResult.Builder();
  }

  public abstract Optional<Report> report();

  public abstract ImmutableList<ErrorMessage> errorMessages();

  /**
   * Validation helper
   *
   * <p>Validates that either report or errors are present, but not both, and that one of them must
   * be set.
   */
  private boolean isValid() {
    boolean errorsPresent = !errorMessages().isEmpty();
    return (report().isPresent() || errorsPresent) && !(report().isPresent() && errorsPresent);
  }

  @AutoValue.Builder
  public abstract static class Builder {

    public abstract Builder setReport(Report report);

    abstract ImmutableList.Builder<ErrorMessage> errorMessagesBuilder();

    public Builder addErrorMessage(ErrorMessage errorMessage) {
      errorMessagesBuilder().add(errorMessage);
      return this;
    }

    public Builder addAllErrorMessage(Iterable<ErrorMessage> errorMessages) {
      errorMessagesBuilder().addAll(errorMessages);
      return this;
    }

    abstract DecryptionResult autoBuild();

    /** Builds and performs validations */
    public DecryptionResult build() {
      DecryptionResult decryptionResult = autoBuild();
      checkState(
          decryptionResult.isValid(),
          "Cannot have both Report and ErrorMessages present, must have either Report or"
              + " ErrorMessages not both. DecryptionResult as created: "
              + decryptionResult);
      return decryptionResult;
    }
  }
}
