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

package com.google.scp.shared.aws.gateway.error;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonInclude;
import com.fasterxml.jackson.annotation.JsonInclude.Include;
import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.databind.annotation.JsonDeserialize;
import com.google.auto.value.AutoValue;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.scp.shared.api.model.Code;
import java.util.List;
import javax.annotation.Nullable;

/**
 * Java representation of google.rpc Error defined in https://cloud.google.com/apis/design/errors
 */
@AutoValue
@JsonDeserialize(builder = ErrorResponse.Builder.class)
public abstract class ErrorResponse {

  public static Builder builder() {
    return new AutoValue_ErrorResponse.Builder();
  }

  /**
   * A simple error code that can be easily handled by the client. Corresponds to rpcStatusCode int
   * of {@link Code} enum
   */
  @JsonProperty("code")
  public abstract int code();

  /**
   * A developer-facing human-readable error message in English. It should both explain the error
   * and offer an actionable resolution to it.
   */
  @JsonProperty("message")
  public abstract String message();

  /**
   * Additional error information that the client code can use to handle the error. Adheres to
   * ErrorInfo in google.rpc.ErrorInfo
   */
  @JsonProperty("details")
  public abstract ImmutableList<Details> details();

  @AutoValue.Builder
  public abstract static class Builder {

    @JsonCreator
    public static Builder builder() {
      return new AutoValue_ErrorResponse.Builder();
    }

    @JsonProperty("code")
    public abstract Builder setCode(int code);

    @JsonProperty("message")
    public abstract Builder setMessage(String message);

    @JsonProperty("details")
    public abstract Builder setDetails(List<Details> details);

    public abstract ErrorResponse build();
  }

  @AutoValue
  @JsonDeserialize(builder = Details.Builder.class)
  public abstract static class Details {

    public static Builder builder() {
      return new AutoValue_ErrorResponse_Details.Builder();
    }

    /**
     * The reason of the error. This is a constant value that identifies the proximate cause of the
     * error.
     */
    @JsonProperty("reason")
    public abstract String reason();

    /**
     * The error domain is typically the registered service name of the tool or product that
     * generates the error. Example: "pubsub.googleapis.com".
     */
    @Nullable
    @JsonInclude(Include.NON_NULL)
    @JsonProperty("domain")
    public abstract String domain();

    /** Additional structured details about this error */
    @Nullable
    @JsonInclude(Include.NON_NULL)
    @JsonProperty("metadata")
    public abstract ImmutableMap<String, String> metadata();

    @AutoValue.Builder
    public abstract static class Builder {

      @JsonCreator
      public static Builder builder() {
        return new AutoValue_ErrorResponse_Details.Builder();
      }

      @JsonProperty("reason")
      public abstract Builder setReason(String reason);

      @JsonProperty("domain")
      public abstract Builder setDomain(String domain);

      @JsonProperty("metadata")
      public abstract Builder setMetadata(ImmutableMap<String, String> metadata);

      public abstract Details build();
    }
  }
}
