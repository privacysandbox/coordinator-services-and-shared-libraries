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

import com.google.common.collect.ImmutableList;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.aws.gateway.error.ErrorResponse.Details;
import java.util.stream.Collectors;

/** Defines helper methods related to errors and exceptions */
public final class ErrorUtil {

  private ErrorUtil() {}

  /**
   * Converts ServiceException to ErrorResponse which can be used as a response body, following gRpc
   * style
   *
   * @param exception ServiceException with nonnull errorCode, errorReason, and message
   * @return ErrorResponse which can be serialized to a JSON response body
   */
  public static ErrorResponse toErrorResponse(ServiceException exception) {
    Code code = exception.getErrorCode();
    return ErrorResponse.builder()
        .setCode(code.getRpcStatusCode())
        .setMessage(exception.getMessage())
        .setDetails(
            ImmutableList.of(Details.builder().setReason(exception.getErrorReason()).build()))
        .build();
  }

  /**
   * Reconstructs a ServiceException from an ErrorResponse, providing an exception that can be
   * thrown (or wrapped) when a service client encounters an error.
   */
  public static ServiceException toServiceException(ErrorResponse response) {
    var code = Code.fromRpcStatusCode(response.code());
    var message = response.message();
    var reason = response.details().stream().map(Details::reason).collect(Collectors.joining("\n"));

    return new ServiceException(code, reason, message);
  }
}
