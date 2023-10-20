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

package com.google.scp.shared.clients.configclient.gcp;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.databind.annotation.JsonDeserialize;
import com.fasterxml.jackson.databind.annotation.JsonSerialize;
import com.google.auto.value.AutoValue;

/** Represents source of credential in CredentialConfig. */
@AutoValue
@JsonDeserialize(builder = CredentialSource.Builder.class)
@JsonSerialize(as = CredentialSource.class)
public abstract class CredentialSource {

  /** Returns an instance of the {@code CredentialSource.Builder} class. */
  public static Builder builder() {
    return Builder.builder();
  }

  /**
   * Returns an instance of the {@code CredentialSource.Builder} class set with the same field
   * values as the {@code CredentialSource} instance.
   */
  public abstract Builder toBuilder();

  /** Returns the file path. */
  @JsonProperty("file")
  public abstract String file();

  /** Builder class for the {@code CredentialSource} class. */
  @AutoValue.Builder
  public abstract static class Builder {

    /** Returns a new instance of the builder. */
    @JsonCreator
    public static Builder builder() {
      return new AutoValue_CredentialSource.Builder();
    }

    /** Set the file path. */
    @JsonProperty("file")
    public abstract Builder file(String file);

    /** Uses the builder to construct a new instance of the {@code CredentialSource} class. */
    public abstract CredentialSource build();
  }
}
