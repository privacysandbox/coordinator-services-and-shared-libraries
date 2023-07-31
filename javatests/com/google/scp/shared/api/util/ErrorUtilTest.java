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

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static com.google.scp.shared.api.util.ErrorUtil.toErrorResponse;

import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.Details;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class ErrorUtilTest {

  @Test
  public void errorResponse() {
    ServiceException exception = new ServiceException(INTERNAL, "SERVICE_ERROR", "error");

    ErrorResponse response = toErrorResponse(exception);

    assertThat(response).isNotNull();
    assertThat(response).isNotNull();
    assertThat(response.getCode()).isEqualTo(INTERNAL.getRpcStatusCode());
    assertThat(response.getMessage()).isEqualTo("error");
    assertThat(response.getDetailsList()).isNotEmpty();
    assertThat(response.getDetailsList().get(0).getReason()).isEqualTo("SERVICE_ERROR");
  }

  @Test
  public void errorResponse_serialize() throws InvalidProtocolBufferException {
    ServiceException exception = new ServiceException(INTERNAL, "SERVICE_ERROR", "error");

    ErrorResponse response = toErrorResponse(exception);
    String responseJson = JsonFormat.printer().print(response);

    assertThat(responseJson)
        .isEqualTo(expectedErrorBody(INTERNAL.getRpcStatusCode(), "error", "SERVICE_ERROR"));
  }

  @Test
  public void parseErrorResponse_success() throws Exception {
    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage("parseErrorResponse")
            .build();
    String json = JsonFormat.printer().print(errorResponse);

    ErrorResponse result = ErrorUtil.parseErrorResponse(json);
    assertThat(result).isEqualTo(errorResponse);
  }

  @Test
  public void parseErrorResponse_missingCode() throws Exception {
    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setMessage("parseErrorResponse_failed")
            .addDetails(Details.newBuilder().setReason("missingCode"))
            .build();
    String json = JsonFormat.printer().print(errorResponse);

    ErrorResponse result = ErrorUtil.parseErrorResponse(json);
    assertThat(result.getCode()).isEqualTo(Code.UNKNOWN.getRpcStatusCode());
    assertThat(result.getMessage()).isEqualTo(json);
    assertThat(result.getDetailsList()).isEmpty();
  }

  @Test
  public void parseErrorResponse_missingMessage() throws Exception {
    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .addDetails(Details.newBuilder().setReason("missingMessage"))
            .build();
    String json = JsonFormat.printer().print(errorResponse);

    ErrorResponse result = ErrorUtil.parseErrorResponse(json);
    assertThat(result.getCode()).isEqualTo(Code.UNKNOWN.getRpcStatusCode());
    assertThat(result.getMessage()).isEqualTo(json);
    assertThat(result.getDetailsList()).isEmpty();
  }

  @Test
  public void parseErrorResponse_invalidResponse() throws Exception {
    String json = "{ \"foo\": \"bar\", \"alpha\": \"beta\" }";

    ErrorResponse result = ErrorUtil.parseErrorResponse(json);
    assertThat(result.getCode()).isEqualTo(Code.UNKNOWN.getRpcStatusCode());
    assertThat(result.getMessage()).isEqualTo(json);
    assertThat(result.getDetailsList()).isEmpty();
  }

  private static String expectedErrorBody(int rpcStatusCode, String message, String reason) {
    return String.format(
        "{\n"
            + "  \"code\": %d,\n"
            + "  \"message\": \"%s\",\n"
            + "  \"details\": [{\n"
            + "    \"reason\": \"%s\"\n"
            + "  }]\n"
            + "}",
        rpcStatusCode, message, reason);
  }
}
