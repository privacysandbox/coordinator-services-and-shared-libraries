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

package com.google.scp.shared.gcp.util;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.expectedEncodedPublicKey;
import static com.google.scp.coordinator.keymanagement.testutils.InMemoryKeyDbTestUtil.expectedErrorResponseBody;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.api.model.Code.UNKNOWN;
import static com.google.scp.shared.api.util.ErrorUtil.toErrorResponse;
import static com.google.scp.shared.gcp.util.CloudFunctionUtil.createCloudFunctionResponseFromProto;
import static com.google.scp.shared.mapper.TimeObjectMapper.SERIALIZATION_ERROR_RESPONSE_JSON;
import static java.util.Collections.emptyMap;
import static java.util.UUID.randomUUID;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.cloud.functions.HttpResponse;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.protobuf.Any;
import com.google.protobuf.ByteString;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.EncodedPublicKeyProto.EncodedPublicKey;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysResponseProto.GetActivePublicKeysResponse;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.exception.ServiceException;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.StringWriter;
import java.util.Map;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class CloudFunctionUtilTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  private BufferedWriter writerOut;
  private StringWriter httpResponseOut;
  @Mock private HttpResponse httpResponse;

  @Before
  public void setUp() throws IOException {
    httpResponseOut = new StringWriter();
    writerOut = new BufferedWriter(httpResponseOut);
    when(httpResponse.getWriter()).thenReturn(writerOut);
  }

  @Test
  public void createCloudFunctionResponse_getActivePublicKeysResponseWithHeaders()
      throws IOException {
    String keyId = randomUUID().toString();
    String publicKey = randomUUID().toString();
    Map<String, String> testHeaders =
        ImmutableMap.of(
            /* k0=*/ "header0",
            /* v0=*/ randomUUID().toString(),
            /* k1=*/ "header1",
            /* v1=*/ randomUUID().toString());

    GetActivePublicKeysResponse response = getRandomActiveKeysResponse(keyId, publicKey);

    createCloudFunctionResponseFromProto(
        httpResponse, response, OK.getHttpStatusCode(), testHeaders);
    writerOut.flush();

    GetActivePublicKeysResponse.Builder builder = GetActivePublicKeysResponse.newBuilder();
    JsonFormat.parser().merge(httpResponseOut.toString(), builder);
    assertThat(builder.build().getKeys(0)).isEqualTo(expectedEncodedPublicKey(keyId, publicKey));
    verify(httpResponse).appendHeader(eq("header0"), eq(testHeaders.get("header0")));
    verify(httpResponse).appendHeader(eq("header1"), eq(testHeaders.get("header1")));
  }

  @Test
  public void createCloudFunctionResponse_getActivePublicKeysResponseWithoutHeaders()
      throws IOException {
    String keyId = randomUUID().toString();
    String publicKey = randomUUID().toString();
    GetActivePublicKeysResponse response = getRandomActiveKeysResponse(keyId, publicKey);

    createCloudFunctionResponseFromProto(
        httpResponse, response, OK.getHttpStatusCode(), ImmutableMap.of());
    writerOut.flush();

    GetActivePublicKeysResponse.Builder builder = GetActivePublicKeysResponse.newBuilder();
    JsonFormat.parser().merge(httpResponseOut.toString(), builder);
    assertThat(builder.build().getKeys(0)).isEqualTo(expectedEncodedPublicKey(keyId, publicKey));
    verify(httpResponse, times(0)).appendHeader(anyString(), anyString());
  }

  @Test
  public void createCloudFunctionResponse_errorResponse() throws IOException {
    ServiceException exception = new ServiceException(UNKNOWN, "SERVICE_ERROR", "test_error");
    ErrorResponse response = toErrorResponse(exception);

    createCloudFunctionResponseFromProto(
        httpResponse, response, UNKNOWN.getHttpStatusCode(), emptyMap());
    writerOut.flush();

    assertThat(httpResponseOut.toString())
        .isEqualTo(
            expectedErrorResponseBody(
                /* code = */ UNKNOWN.getRpcStatusCode(),
                /* message = */ "test_error",
                /* reason = */ "SERVICE_ERROR"));
    verify(httpResponse).setStatusCode(eq(UNKNOWN.getHttpStatusCode()));
  }

  @Test
  public void createCloudFunctionResponse_invalidProtocolBufferException() throws IOException {
    Any invalidProto =
        Any.newBuilder().setTypeUrl("UNKNOWN").setValue(ByteString.copyFromUtf8("1")).build();
    createCloudFunctionResponseFromProto(
        httpResponse, invalidProto, OK.getHttpStatusCode(), emptyMap());
    writerOut.flush();

    assertThat(httpResponseOut.toString()).isEqualTo(SERIALIZATION_ERROR_RESPONSE_JSON);
    verify(httpResponse).setStatusCode(eq(INTERNAL.getHttpStatusCode()));
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

  private static class UnserializableTestClass {}
}
