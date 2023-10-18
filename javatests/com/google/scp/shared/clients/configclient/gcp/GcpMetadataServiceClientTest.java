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

package com.google.scp.shared.clients.configclient.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.IOException;
import java.util.Optional;
import org.apache.http.HttpResponse;
import org.apache.http.ProtocolVersion;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpUriRequest;
import org.apache.http.entity.StringEntity;
import org.apache.http.message.BasicStatusLine;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class GcpMetadataServiceClientTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Mock private HttpClient httpClient;

  private GcpMetadataServiceClient client;

  @Before
  public void setUp() {
    client = new GcpMetadataServiceClient(httpClient);
  }

  @Test
  public void getProjectId_success() throws IOException {
    String projectId = "anyProjectIdString";
    HttpResponse fakeResponse = buildFakeResponse(200, projectId);
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(fakeResponse);

    String result = client.getGcpProjectId();

    assertThat(result).isEqualTo(projectId);
    verify(httpClient).execute(any(HttpUriRequest.class));
  }

  @Test
  public void getInstanceId_success() throws IOException {
    String instanceIdString = "anyInstanceIdString";
    HttpResponse fakeResponse = buildFakeResponse(200, instanceIdString);
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(fakeResponse);

    String result = client.getGcpInstanceId();

    assertThat(result).isEqualTo(instanceIdString);
    verify(httpClient).execute(any(HttpUriRequest.class));
  }

  @Test
  public void getZone_success() throws IOException {
    String zone = "projects/123123213/zones/us-west4-a";
    HttpResponse fakeResponse = buildFakeResponse(200, zone);
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(fakeResponse);

    String result = client.getGcpZone();

    assertThat(result).isEqualTo("us-west4-a");
    verify(httpClient).execute(any(HttpUriRequest.class));
  }

  @Test
  public void getMetadata_success() throws IOException {
    String response = "test-response";
    HttpResponse fakeResponse = buildFakeResponse(200, response);
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(fakeResponse);

    Optional<String> result = client.getMetadata("test-key");

    assertThat(result).isPresent();
    assertThat(result.get()).isEqualTo(response);
    verify(httpClient).execute(any(HttpUriRequest.class));
  }

  @Test
  public void getMetadata_notFound() throws IOException {
    HttpResponse fakeResponse = buildFakeResponse(404, "Not found");
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(fakeResponse);

    Optional<String> result = client.getMetadata("test-key");

    assertThat(result).isEmpty();
    verify(httpClient).execute(any(HttpUriRequest.class));
  }

  @Test
  public void getMetadata_unexpectedErrorCode() throws IOException {
    HttpResponse fakeResponse = buildFakeResponse(403, "Forbidden");
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(fakeResponse);

    assertThrows(IOException.class, () -> client.getMetadata("test-key"));
    verify(httpClient).execute(any(HttpUriRequest.class));
  }

  private HttpResponse buildFakeResponse(int statusCode, String body) throws IOException {
    HttpResponse response = mock(HttpResponse.class);
    var protocol = new ProtocolVersion("HTTP", 1, 1);
    lenient()
        .when(response.getStatusLine())
        .thenReturn(new BasicStatusLine(protocol, statusCode, ""));
    lenient().when(response.getEntity()).thenReturn(new StringEntity(body));

    return response;
  }
}
