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

package com.google.scp.coordinator.keymanagement.keyhosting.service.common;

import com.google.auto.value.AutoValue;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysResponseProto.GetActivePublicKeysResponse;
import java.util.Optional;

/** Wrapper around {@link GetActivePublicKeysResponse} that contains non-serialized headers */
@AutoValue
public abstract class GetActivePublicKeysResponseWithHeaders {

  public abstract GetActivePublicKeysResponse getActivePublicKeysResponse();

  /** Transient field used as Cache-Control header value. Must be in 'max-age=%seconds' format */
  public abstract Optional<String> cacheControlMaxAge();

  public static GetActivePublicKeysResponseWithHeaders.Builder builder() {
    return new AutoValue_GetActivePublicKeysResponseWithHeaders.Builder();
  }

  @AutoValue.Builder
  public abstract static class Builder {

    public abstract GetActivePublicKeysResponseWithHeaders.Builder setGetActivePublicKeysResponse(
        GetActivePublicKeysResponse getActivePublicKeysResponse);

    public abstract GetActivePublicKeysResponseWithHeaders.Builder setCacheControlMaxAge(
        String value);

    public abstract GetActivePublicKeysResponseWithHeaders build();
  }
}
