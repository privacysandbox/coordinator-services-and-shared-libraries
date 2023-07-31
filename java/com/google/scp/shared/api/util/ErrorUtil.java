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

package com.google.scp.shared.api.util;

import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.Details;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.util.List;
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
    return ErrorResponse.newBuilder()
        .setCode(code.getRpcStatusCode())
        .setMessage(exception.getMessage())
        .addAllDetails(List.of(Details.newBuilder().setReason(exception.getErrorReason()).build()))
        .build();
  }

  /**
   * Reconstructs a ServiceException from an ErrorResponse, providing an exception that can be
   * thrown (or wrapped) when a service client encounters an error.
   */
  public static ServiceException toServiceException(ErrorResponse response) {
    var code = Code.fromRpcStatusCode(response.getCode());
    var message = response.getMessage();
    var reason =
        response.getDetailsList().stream()
            .map(Details::getReason)
            .collect(Collectors.joining("\n"));

    return new ServiceException(code, reason, message);
  }

  /**
   * Attempts to read the body of a non-200 response and convert it to an {@link ErrorResponse}
   * instance.
   */
  public static ErrorResponse parseErrorResponse(String responseBody) {
    try {
      ErrorResponse.Builder builder = ErrorResponse.newBuilder();
      JsonFormat.parser().merge(responseBody, builder);
      ErrorResponse errorResponse = builder.build();

      if (errorResponse.getCode() != 0 && !errorResponse.getMessage().isEmpty()) {
        return errorResponse;
      }
      // ErrorResponse not having code or message means it's an error that didn't originate from us.
      // Thus treat it as an unknown error response.
      return buildUnknownErrorResponse(responseBody);
    } catch (InvalidProtocolBufferException e) {
      return buildUnknownErrorResponse(responseBody);
    }
  }

  private static ErrorResponse buildUnknownErrorResponse(String responseBody) {
    return ErrorResponse.newBuilder()
        .setCode(Code.UNKNOWN.getRpcStatusCode())
        .setMessage(responseBody)
        .addAllDetails(List.of())
        .build();
  }
}
