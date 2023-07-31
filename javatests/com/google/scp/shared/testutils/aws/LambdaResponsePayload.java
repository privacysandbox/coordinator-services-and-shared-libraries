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

package com.google.scp.shared.testutils.aws;

import static com.fasterxml.jackson.annotation.JsonInclude.Include.NON_NULL;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonInclude;
import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.databind.annotation.JsonDeserialize;
import com.fasterxml.jackson.databind.annotation.JsonSerialize;
import com.google.auto.value.AutoValue;
import java.util.Map;
import javax.annotation.Nullable;

/**
 * Defines POJO for Lambda Payload. This specific representation is only visible when invoking the
 * lambda directly, for example with LocalStack
 */
@JsonDeserialize(builder = LambdaResponsePayload.Builder.class)
@JsonSerialize(as = LambdaResponsePayload.class)
@AutoValue
public abstract class LambdaResponsePayload {

  @JsonProperty("body")
  public abstract String body();

  @JsonProperty("statusCode")
  public abstract int statusCode();

  @Nullable
  @JsonInclude(NON_NULL)
  @JsonProperty("headers")
  public abstract Map<String, String> headers();

  public static Builder builder() {
    return Builder.builder();
  }

  @AutoValue.Builder
  public abstract static class Builder {

    @JsonCreator
    public static Builder builder() {
      return new AutoValue_LambdaResponsePayload.Builder();
    }

    @JsonProperty("body")
    public abstract Builder setBody(String body);

    @JsonProperty("statusCode")
    public abstract Builder setStatusCode(int statusCode);

    @JsonProperty("headers")
    public abstract Builder setHeaders(Map<String, String> headers);

    public abstract LambdaResponsePayload build();
  }
}
