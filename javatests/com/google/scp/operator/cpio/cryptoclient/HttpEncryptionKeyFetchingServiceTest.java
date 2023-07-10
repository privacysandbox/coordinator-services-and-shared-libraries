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

package com.google.scp.operator.cpio.cryptoclient;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static com.google.scp.shared.api.model.Code.PERMISSION_DENIED;
import static com.google.scp.shared.api.model.Code.UNKNOWN;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.scp.operator.cpio.cryptoclient.EncryptionKeyFetchingService.EncryptionKeyFetchingServiceException;
import com.google.scp.shared.api.exception.ServiceException;
import java.io.IOException;
import java.net.URI;
import org.apache.http.HttpResponse;
import org.apache.http.ProtocolVersion;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpUriRequest;
import org.apache.http.entity.StringEntity;
import org.apache.http.message.BasicStatusLine;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

/**
 * Tests that SplitKeyFetchingService works when provided fake {@link HttpResponse} objects
 * containing responses from real HTTP requests to the live Private Key Vending service.
 */
@RunWith(JUnit4.class)
public final class HttpEncryptionKeyFetchingServiceTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private HttpClient httpClient;

  @Test
  @SuppressWarnings("unchecked") // Ignore Http{Request,Response,Client} casting warnings.
  public void fetchEncryptionKey_notFound() throws Exception {
    var response = buildFakeResponse(404, getNotFoundResponse());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    var service = new HttpEncryptionKeyFetchingService(httpClient, "https://example.com/v1");
    EncryptionKeyFetchingServiceException exception =
        assertThrows(
            EncryptionKeyFetchingServiceException.class,
            () -> service.fetchEncryptionKey("03a0c66b-e8ff-42db-a750-31009c5894f9"));

    assertThat(exception).hasCauseThat().isInstanceOf(ServiceException.class);
    ServiceException e = (ServiceException) exception.getCause();
    assertThat(e.getErrorCode()).isEqualTo(NOT_FOUND);
  }

  @Test
  @SuppressWarnings("unchecked") // Ignore Http{Request,Response,Client} casting warnings.
  public void fetchEncryptionKey_forbidden() throws Exception {
    var response = buildFakeResponse(PERMISSION_DENIED.getHttpStatusCode(), getForbiddenResponse());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    var service = new HttpEncryptionKeyFetchingService(httpClient, "https://example.com/v1");
    EncryptionKeyFetchingServiceException exception =
        assertThrows(
            EncryptionKeyFetchingServiceException.class,
            () -> service.fetchEncryptionKey("03a0c66b-e8ff-42db-a750-31009c5894f9"));

    assertThat(exception).hasCauseThat().isInstanceOf(ServiceException.class);
    ServiceException e = (ServiceException) exception.getCause();
    assertThat(e.getMessage()).contains(getForbiddenResponse());
    assertThat(e.getErrorCode()).isEqualTo(UNKNOWN); // Due to absence of code field in JSON
    assertThat(e.getErrorReason()).contains("");
  }

  @Test
  @SuppressWarnings("unchecked") // Ignore Http{Request,Response,Client} casting warnings.
  public void fetchEncryptionKey_unsupportedKeyTypeResponse() throws Exception {
    var response = buildFakeResponse(200, getUnsupportedKeyTypeResponse());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    var service = new HttpEncryptionKeyFetchingService(httpClient, "https://example.com/v1");
    EncryptionKeyFetchingServiceException exception =
        assertThrows(
            EncryptionKeyFetchingServiceException.class, () -> service.fetchEncryptionKey("12345"));
  }

  @Test
  @SuppressWarnings("unchecked") // Ignore Http{Request,Response,Client} casting warnings.
  public void fetchEncryptionKey_success() throws Exception {
    var response = buildFakeResponse(200, getSingleKeyResponse());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);
    var expectedUri = URI.create("https://example.com/v1/encryptionKeys/12345");
    var argument = ArgumentCaptor.forClass(HttpUriRequest.class);

    var service = new HttpEncryptionKeyFetchingService(httpClient, "https://example.com/v1");
    // Assume response is valid if deserialization was successful.
    service.fetchEncryptionKey("12345");

    verify(httpClient).execute(argument.capture());
    assertThat(argument.getValue().getURI()).isEqualTo(expectedUri);
  }

  @SuppressWarnings("unchecked")
  private HttpResponse buildFakeResponse(int statusCode, String body) throws IOException {
    HttpResponse response = mock(HttpResponse.class);
    var protocol = new ProtocolVersion("HTTP", 1, 1);
    when(response.getStatusLine()).thenReturn(new BasicStatusLine(protocol, statusCode, ""));
    when(response.getEntity()).thenReturn(new StringEntity(body));

    return response;
  }

  private static String getForbiddenResponse() {
    return "{\"message\":\"Forbidden\"}";
  }

  private static String getNotFoundResponse() {
    return "{\"code\":5,\"message\":\"Unable to find item with keyId"
        + " 03a0c66b-e8ff-42db-a750-31009c5894f8\",\"details\":[{\"reason\":\"MISSING_KEY\"}]}";
  }

  private static String getUnsupportedKeyTypeResponse() {
    return "{\n"
        + "    \"name\": \"encryptionKeys/12345\",\n"
        + "    \"encryptionKeyType\": \"UNSUPPORTED_KEY_TYPE\",\n"
        + "    \"publicKeysetHandle\": \"myPublicKeysetHandle\",\n"
        + "    \"publicKeyMaterial\": \"abc==\",\n"
        + "    \"keyData\": [\n"
        + "        {\n"
        + "            \"publicKeySignature\": \"a\",\n"
        + "            \"keyEncryptionKeyUri\": \"b\",\n"
        + "            \"keyMaterial\": \"efg==\"\n"
        + "        }\n"
        + "    ]\n"
        + "}";
  }

  private static String getSingleKeyResponse() {
    return "{\n"
        + "    \"name\": \"encryptionKeys/12345\",\n"
        + "    \"encryptionKeyType\": \"SINGLE_PARTY_HYBRID_KEY\",\n"
        + "    \"publicKeysetHandle\": \"myPublicKeysetHandle\",\n"
        + "    \"publicKeyMaterial\": \"abc==\",\n"
        + "    \"keyData\": [\n"
        + "        {\n"
        + "            \"publicKeySignature\": \"a\",\n"
        + "            \"keyEncryptionKeyUri\": \"b\",\n"
        + "            \"keyMaterial\": \"efg==\"\n"
        + "        }\n"
        + "    ]\n"
        + "}";
  }
}
