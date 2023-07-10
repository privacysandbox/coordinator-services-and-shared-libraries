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

import com.google.scp.operator.cpio.cryptoclient.PrivateKeyFetchingService.PrivateKeyFetchingServiceException;
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
 * Tests that HttpPrivateKeyFetchingService works when provided fake {@link HttpResponse} objects
 * containing responses from real HTTP requests to the live Private Key Vending service.
 *
 * <p>TODO: Replace or supplement these tests with tests that create a real Private Key Vending
 * service.
 */
@RunWith(JUnit4.class)
public final class HttpPrivateKeyFetchingServiceTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private HttpClient httpClient;

  @Test
  @SuppressWarnings("unchecked") // Ignore Http{Request,Response,Client} casting warnings.
  public void fetchKeyCiphertext_notFound() throws Exception {
    var response = buildFakeResponse(404, getNotFoundResponse());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    var service = new HttpPrivateKeyFetchingService("https://example.com/v1", httpClient);
    PrivateKeyFetchingServiceException exception =
        assertThrows(
            PrivateKeyFetchingServiceException.class,
            () -> service.fetchKeyCiphertext("03a0c66b-e8ff-42db-a750-31009c5894f9"));

    assertThat(exception).hasCauseThat().isInstanceOf(ServiceException.class);
    ServiceException e = (ServiceException) exception.getCause();
    assertThat(e.getErrorCode()).isEqualTo(NOT_FOUND);
  }

  @Test
  @SuppressWarnings("unchecked") // Ignore Http{Request,Response,Client} casting warnings.
  public void fetchKeyCiphertext_forbidden() throws Exception {
    var response = buildFakeResponse(PERMISSION_DENIED.getHttpStatusCode(), getForbiddenResponse());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    var service = new HttpPrivateKeyFetchingService("https://example.com/v1", httpClient);
    PrivateKeyFetchingServiceException exception =
        assertThrows(
            PrivateKeyFetchingServiceException.class,
            () -> service.fetchKeyCiphertext("03a0c66b-e8ff-42db-a750-31009c5894f9"));

    assertThat(exception).hasCauseThat().isInstanceOf(ServiceException.class);
    ServiceException e = (ServiceException) exception.getCause();
    assertThat(e.getMessage()).contains(getForbiddenResponse());
    assertThat(e.getErrorCode()).isEqualTo(UNKNOWN); // Due to absence of code field in JSON
    assertThat(e.getErrorReason()).contains("");
  }

  @Test
  @SuppressWarnings("unchecked") // Ignore Http{Request,Response,Client} casting warnings.
  public void fetchKeyCiphertext_success() throws Exception {
    var response = buildFakeResponse(200, getSuccessResponse());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);
    var expectedUri = URI.create("https://example.com/v2alpha/privateKeys/abc");
    var argument = ArgumentCaptor.forClass(HttpUriRequest.class);

    var service = new HttpPrivateKeyFetchingService("https://example.com/v2alpha", httpClient);
    // Assume response is valid if deserialization was successful.
    service.fetchKeyCiphertext("abc");

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

  private static String getSuccessResponse() {
    return "{\"name\":\"privateKeys/03a0c66b-e8ff-42db-a750-31009c5894f9\",\"jsonEncodedKeyset\":\"{\\n"
               + "    \\\"keysetInfo\\\": {\\n"
               + "        \\\"primaryKeyId\\\": 509395462,\\n"
               + "        \\\"keyInfo\\\": [{\\n"
               + "            \\\"typeUrl\\\":"
               + " \\\"type.googleapis.com/google.crypto.tink.EciesAeadHkdfPrivateKey\\\",\\n"
               + "            \\\"outputPrefixType\\\": \\\"TINK\\\",\\n"
               + "            \\\"keyId\\\": 509395462,\\n"
               + "            \\\"status\\\": \\\"ENABLED\\\"\\n"
               + "        }]\\n"
               + "    },\\n"
               + "    \\\"encryptedKeyset\\\":"
               + " \\\"AQICAHiuStyq7W9/DQFL6rofWENTET/4FcRss+J9ka6bEiuJbwG+oJ/qfrdfOvgg7RHiQpvLAAABczCCAW8GCSqGSIb3DQEHBqCCAWAwggFcAgEAMIIBVQYJKoZIhvcNAQcBMB4GCWCGSAFlAwQBLjARBAywU6e3WeACDz19qC0CARCAggEmV/smghyzb8pBOTZWSt158dv8PBUNKlgFjMhwIZyON3gwDb70QHEaQkDnv8dguC1D+FghbyBCD5rh3eDyq6l3Pyc4OuoaroHSMy+2lhfotnyHAID0uwLhW6Qm+wFx0oo3DbFNjNgtMHWfRr5PE9HxdR62Xn8kFRvspWKnebdXx4QdqYCPnrZHXYsrLLQAJfNJyXXQIyszk87cg9aRkd49N59eO+pAaSXiVA/7FwAoCQyOAISlrFAau02cmH9+lWB88HcNbqLi/wWGO5Qy7at2RRR3HJx6vdgt1GImjydfHZkRB95rgLO2ikBoBhy0h60Z9bVEP3gT085ce4PVJVXrIrWVxV+L6mPqOVmw1E5PBgZ4K8IujiVnLPfJbajZIhN4dh3gd3OE\\\"\\n"
               + "}\\n"
               + "\"}";
  }
}
