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
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.common.collect.ImmutableMap;
import com.google.scp.operator.cpio.cryptoclient.EncryptionKeyFetchingService.EncryptionKeyFetchingServiceException;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.util.HttpClientResponse;
import com.google.scp.shared.api.util.HttpClientWrapper;
import java.net.URI;
import org.apache.http.HttpResponse;
import org.apache.http.client.methods.HttpRequestBase;
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

  @Mock private HttpClientWrapper httpClient;

  @Test
  @SuppressWarnings("unchecked") // Ignore Http{Request,Response,Client} casting warnings.
  public void fetchEncryptionKey_notFound() throws Exception {
    HttpClientResponse response =
        HttpClientResponse.create(404, getNotFoundResponse(), ImmutableMap.of());
    when(httpClient.execute(any(HttpRequestBase.class))).thenReturn(response);

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
    HttpClientResponse response =
        HttpClientResponse.create(
            PERMISSION_DENIED.getHttpStatusCode(), getForbiddenResponse(), ImmutableMap.of());
    when(httpClient.execute(any(HttpRequestBase.class))).thenReturn(response);

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
  public void fetchEncryptionKey_success() throws Exception {
    HttpClientResponse response =
        HttpClientResponse.create(200, getSingleKeyResponse(), ImmutableMap.of());
    when(httpClient.execute(any(HttpRequestBase.class))).thenReturn(response);
    var expectedUri = URI.create("https://example.com/v1/encryptionKeys/12345");
    var argument = ArgumentCaptor.forClass(HttpRequestBase.class);

    var service = new HttpEncryptionKeyFetchingService(httpClient, "https://example.com/v1");
    // Assume response is valid if deserialization was successful.
    service.fetchEncryptionKey("12345");

    verify(httpClient).execute(argument.capture());
    assertThat(argument.getValue().getURI()).isEqualTo(expectedUri);
  }

  private static String getForbiddenResponse() {
    return "{\"message\":\"Forbidden\"}";
  }

  private static String getNotFoundResponse() {
    return "{\"code\":5,\"message\":\"Unable to find item with keyId"
        + " 03a0c66b-e8ff-42db-a750-31009c5894f8\",\"details\":[{\"reason\":\"MISSING_KEY\"}]}";
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
