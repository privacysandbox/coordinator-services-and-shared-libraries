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

import static java.nio.charset.StandardCharsets.UTF_8;

import com.google.inject.Inject;
import java.io.IOException;
import java.util.Optional;
import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.util.EntityUtils;

/** Client for accessing GCP metadata. */
public class GcpMetadataServiceClient {

  private static final String GCP_PROJECT_ID_ENDPOINT =
      "http://metadata.google.internal/computeMetadata/v1/project/project-id";
  private static final String GCP_METADATA_ENDPOINT_PATTERN =
      "http://metadata.google.internal/computeMetadata/v1/instance/attributes/%s";
  private static final String GCP_INSTANCE_ID_ENDPOINT =
      "http://metadata.google.internal/computeMetadata/v1/instance/id";
  private static final String GCP_INSTANCE_NAME_ENDPOINT =
      "http://metadata.google.internal/computeMetadata/v1/instance/name";
  private static final String GCP_ZONE_ENDPOINT =
      "http://metadata.google.internal/computeMetadata/v1/instance/zone";
  private final HttpClient httpClient;

  @Inject
  public GcpMetadataServiceClient(HttpClient httpClient) {
    this.httpClient = httpClient;
  }

  /** Returns the GCP project id retrieved from the Google metadata service. */
  public String getGcpProjectId() throws IOException {
    HttpGet request = new HttpGet(GCP_PROJECT_ID_ENDPOINT);
    request.setHeader("Metadata-Flavor", "Google");
    HttpResponse response = httpClient.execute(request);
    return new String(response.getEntity().getContent().readAllBytes(), UTF_8);
  }

  /** Returns the metadata for the associated key, or Optional.empty if key does not exist. */
  public Optional<String> getMetadata(String key) throws IOException {
    String metadataEndpoint = String.format(GCP_METADATA_ENDPOINT_PATTERN, key);
    HttpGet request = new HttpGet(metadataEndpoint);
    request.setHeader("Metadata-Flavor", "Google");

    HttpResponse httpResponse = httpClient.execute(request);

    int statusCode = httpResponse.getStatusLine().getStatusCode();
    String body = new String(httpResponse.getEntity().getContent().readAllBytes(), UTF_8);
    if (statusCode == HttpStatus.SC_OK) {
      return Optional.of(body);
    } else if (statusCode == HttpStatus.SC_NOT_FOUND) {
      return Optional.empty();
    }

    throw new IOException(
        String.format("Got unexpected status code '%s'. Response: %s", statusCode, body));
  }

  /** Returns the GCP instance id retrieved from the Google metadata service. */
  public String getGcpInstanceId() throws IOException {
    HttpGet request = new HttpGet(GCP_INSTANCE_ID_ENDPOINT);
    request.setHeader("Metadata-Flavor", "Google");
    HttpResponse response = httpClient.execute(request);
    return EntityUtils.toString(response.getEntity());
  }

  /** Returns the GCP instance name retrieved from the Google metadata service. */
  public String getGcpInstanceName() throws IOException {
    HttpGet request = new HttpGet(GCP_INSTANCE_NAME_ENDPOINT);
    request.setHeader("Metadata-Flavor", "Google");
    HttpResponse response = httpClient.execute(request);
    return EntityUtils.toString(response.getEntity());
  }

  /** Returns the GCP zone retrieved from the Google metadata service. */
  public String getGcpZone() throws IOException {
    HttpGet request = new HttpGet(GCP_ZONE_ENDPOINT);
    request.setHeader("Metadata-Flavor", "Google");
    HttpResponse response = httpClient.execute(request);
    String content = EntityUtils.toString(response.getEntity());
    String[] list = content.split("/");
    return list[list.length - 1];
  }
}
