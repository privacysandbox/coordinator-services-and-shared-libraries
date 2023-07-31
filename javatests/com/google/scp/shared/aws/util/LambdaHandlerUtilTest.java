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

package com.google.scp.shared.aws.util;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.api.model.Code.UNKNOWN;
import static com.google.scp.shared.api.util.ErrorUtil.toErrorResponse;
import static com.google.scp.shared.aws.util.LambdaHandlerUtil.createApiGatewayResponseFromProto;
import static com.google.scp.shared.mapper.TimeObjectMapper.SERIALIZATION_ERROR_RESPONSE_JSON;
import static java.util.Collections.emptyMap;
import static java.util.UUID.randomUUID;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.protobuf.Any;
import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.EncodedPublicKeyProto.EncodedPublicKey;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysResponseProto.GetActivePublicKeysResponse;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.exception.ServiceException;
import java.util.Map;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class LambdaHandlerUtilTest {

  @Test
  public void createApiGatewayResponse_getActivePublicKeysResponseWithHeaders()
      throws InvalidProtocolBufferException {
    String keyId = randomUUID().toString();
    String publicKey = randomUUID().toString();
    Map<String, String> testHeaders =
        ImmutableMap.of(
            /* k0=*/ "header0",
            /* v0=*/ randomUUID().toString(),
            /* k1=*/ "header1",
            /* v1=*/ randomUUID().toString());

    GetActivePublicKeysResponse response = getRandomActiveKeysResponse(keyId, publicKey);

    APIGatewayProxyResponseEvent lambdaResponse =
        createApiGatewayResponseFromProto(response, OK.getHttpStatusCode(), testHeaders);

    GetActivePublicKeysResponse.Builder builder = GetActivePublicKeysResponse.newBuilder();
    JsonFormat.parser().merge(lambdaResponse.getBody(), builder);
    assertThat(builder.build().getKeys(0)).isEqualTo(expectedEncodedPublicKey(keyId, publicKey));
    assertThat(lambdaResponse.getHeaders()).containsExactlyEntriesIn(testHeaders);
  }

  @Test
  public void createApiGatewayResponse_getActivePublicKeysResponseWithoutHeaders()
      throws InvalidProtocolBufferException {
    String keyId = randomUUID().toString();
    String publicKey = randomUUID().toString();
    GetActivePublicKeysResponse response = getRandomActiveKeysResponse(keyId, publicKey);

    APIGatewayProxyResponseEvent lambdaResponse =
        createApiGatewayResponseFromProto(response, OK.getHttpStatusCode(), ImmutableMap.of());

    GetActivePublicKeysResponse.Builder builder = GetActivePublicKeysResponse.newBuilder();
    JsonFormat.parser().merge(lambdaResponse.getBody(), builder);
    assertThat(builder.build().getKeys(0)).isEqualTo(expectedEncodedPublicKey(keyId, publicKey));
    assertThat(lambdaResponse.getHeaders()).isNull();
  }

  @Test
  public void createApiGatewayResponse_errorResponse() {
    ServiceException exception = new ServiceException(UNKNOWN, "SERVICE_ERROR", "test_error");
    ErrorResponse response = toErrorResponse(exception);

    APIGatewayProxyResponseEvent lambdaResponse =
        createApiGatewayResponseFromProto(response, UNKNOWN.getHttpStatusCode(), emptyMap());

    assertThat(lambdaResponse.getBody())
        .isEqualTo(
            expectedErrorResponseBody(
                /* code = */ UNKNOWN.getRpcStatusCode(),
                /* message = */ "test_error",
                /* reason = */ "SERVICE_ERROR"));
    assertThat(lambdaResponse.getStatusCode()).isEqualTo(UNKNOWN.getHttpStatusCode());
  }

  @Test
  public void createApiGatewayResponse_invalidProtocolBufferException() {
    Any invalidProto =
        Any.newBuilder().setTypeUrl("UNKNOWN").setValue(ByteString.copyFromUtf8("1")).build();
    APIGatewayProxyResponseEvent lambdaResponse =
        createApiGatewayResponseFromProto(invalidProto, OK.getHttpStatusCode(), emptyMap());

    assertThat(lambdaResponse.getBody()).isEqualTo(SERIALIZATION_ERROR_RESPONSE_JSON);
    assertThat(lambdaResponse.getStatusCode()).isEqualTo(INTERNAL.getHttpStatusCode());
  }

  private static GetActivePublicKeysResponse getRandomActiveKeysResponse(
      String keyId, String publicKey) {
    return GetActivePublicKeysResponse.newBuilder()
        .addAllKeys(
            ImmutableList.<EncodedPublicKey>builder()
                .add(EncodedPublicKey.newBuilder().setId(keyId).setKey(publicKey).build())
                .build())
        .build();
  }

  private static EncodedPublicKey expectedEncodedPublicKey(String keyId, String publicKey) {
    return EncodedPublicKey.newBuilder().setId(keyId).setKey(publicKey).build();
  }

  private static String expectedErrorResponseBody(int code, String message, String reason) {
    return String.format(
        "{\n"
            + "  \"code\": %d,\n"
            + "  \"message\": \"%s\",\n"
            + "  \"details\": [{\n"
            + "    \"reason\": \"%s\",\n"
            + "    \"domain\": \"\",\n"
            + "    \"metadata\": {\n"
            + "    }\n"
            + "  }]\n"
            + "}",
        code, message, reason);
  }

  private static class UnserializableTestClass {}
}
