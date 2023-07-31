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

package com.google.scp.shared.aws.gateway;

import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.aws.gateway.error.ErrorReasons;
import com.google.scp.shared.aws.gateway.error.ErrorResponse;
import com.google.scp.shared.aws.gateway.error.ErrorResponse.Details;
import java.util.List;

/** Contains an unknown error response used for testing. */
public final class UnknownErrorResponse {

  /** Gets an unknown error response. */
  public static ErrorResponse getUnknownErrorResponse() {
    return ErrorResponse.builder()
        .setCode(Code.UNKNOWN.getRpcStatusCode())
        .setMessage(ErrorReasons.SERVER_ERROR.toString())
        .setDetails(
            List.of(Details.builder().setReason(ErrorReasons.SERVER_ERROR.toString()).build()))
        .build();
  }
}
