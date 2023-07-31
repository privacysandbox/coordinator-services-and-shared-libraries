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

package com.google.scp.coordinator.keymanagement.keyhosting.service.model.v1;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.databind.annotation.JsonDeserialize;
import com.fasterxml.jackson.databind.annotation.JsonSerialize;
import com.google.auto.value.AutoValue;
import com.google.common.collect.ImmutableList;

/**
 * Response containing list of active public keys
 *
 * <p>External representation of an {@link
 * com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey}
 */
@JsonDeserialize(builder = GetActivePublicKeysResponse.Builder.class)
@JsonSerialize(as = GetActivePublicKeysResponse.class)
@JsonIgnoreProperties(ignoreUnknown = true)
@AutoValue
public abstract class GetActivePublicKeysResponse {

  @JsonProperty("keys")
  public abstract ImmutableList<EncodedPublicKey> publicKeys();

  public static GetActivePublicKeysResponse.Builder builder() {
    return Builder.builder();
  }

  @JsonIgnoreProperties(ignoreUnknown = true)
  @AutoValue.Builder
  public abstract static class Builder {

    @JsonCreator
    public static Builder builder() {
      return new AutoValue_GetActivePublicKeysResponse.Builder();
    }

    @JsonProperty("keys")
    public abstract Builder setPublicKeys(ImmutableList<EncodedPublicKey> value);

    public abstract GetActivePublicKeysResponse build();
  }
}
