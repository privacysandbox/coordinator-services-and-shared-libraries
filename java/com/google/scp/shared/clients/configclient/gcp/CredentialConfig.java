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

/** Represents credential config required for IAM impersonation. */
@AutoValue
@JsonDeserialize(builder = CredentialConfig.Builder.class)
@JsonSerialize(as = CredentialConfig.class)
public abstract class CredentialConfig {

  /** Returns an instance of the {@code CredentialConfig.Builder} class. */
  public static Builder builder() {
    return Builder.builder();
  }

  /**
   * Returns an instance of the {@code CredentialConfig.Builder} class set with the same field
   * values as the {@code CredentialConfig} instance.
   */
  public abstract Builder toBuilder();

  /** Returns the type. */
  @JsonProperty("type")
  public abstract String type();

  /** Returns the audience. */
  @JsonProperty("audience")
  public abstract String audience();

  /** Returns the subject token type. */
  @JsonProperty("subject_token_type")
  public abstract String subjectTokenType();

  /** Returns the token URL. */
  @JsonProperty("token_url")
  public abstract String tokenUrl();

  /** Returns the credential source. */
  @JsonProperty("credential_source")
  public abstract CredentialSource credentialSource();

  /** Returns the service account impersonation URL. */
  @JsonProperty("service_account_impersonation_url")
  public abstract String serviceAccountImpersonationUrl();

  /** Builder class for the {@code CredentialsConfig} class. */
  @AutoValue.Builder
  public abstract static class Builder {

    /** Returns a new instance of the builder. */
    @JsonCreator
    public static Builder builder() {
      return new AutoValue_CredentialConfig.Builder();
    }

    /** Sets the type. */
    @JsonProperty("type")
    public abstract Builder type(String type);

    /** Sets the audience. */
    @JsonProperty("audience")
    public abstract Builder audience(String audience);

    /** Sets the subject token type. */
    @JsonProperty("subject_token_type")
    public abstract Builder subjectTokenType(String subjectTokenType);

    /** Sets the token url. */
    @JsonProperty("token_url")
    public abstract Builder tokenUrl(String tokenUrl);

    /** Sets the credential source. */
    @JsonProperty("credential_source")
    public abstract Builder credentialSource(CredentialSource credentialSource);

    /** Sets the service account impersonation URL. */
    @JsonProperty("service_account_impersonation_url")
    public abstract Builder serviceAccountImpersonationUrl(String serviceAccountImpersonationUrl);

    /** Uses the builder to construct a new instance of the {@code CredentialConfig} class. */
    public abstract CredentialConfig build();
  }
}
