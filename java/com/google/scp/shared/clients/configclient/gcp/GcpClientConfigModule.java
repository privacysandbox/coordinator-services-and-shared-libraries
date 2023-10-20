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

import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpClientConfigHttpClient;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpClientConfigMetadataServiceClient;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpInstanceId;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpInstanceIdOverride;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpInstanceName;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpInstanceNameOverride;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpProjectId;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpProjectIdOverride;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpZone;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpZoneOverride;
import java.io.IOException;
import org.apache.http.client.HttpClient;
import org.apache.http.impl.client.HttpClients;

/** Provides necessary configurations for GCP services clients. */
public final class GcpClientConfigModule extends AbstractModule {

  public GcpClientConfigModule() {}

  /** Provider for a singleton of a String representing the current GCP project ID. */
  @Provides
  @GcpProjectId
  @Singleton
  String provideProjectId(
      @GcpProjectIdOverride String projectIdOverride,
      @GcpClientConfigMetadataServiceClient GcpMetadataServiceClient gcpMetadataServiceClient)
      throws IOException {
    if (projectIdOverride != null && !projectIdOverride.isEmpty()) {
      return projectIdOverride;
    } else {
      return gcpMetadataServiceClient.getGcpProjectId();
    }
  }

  /** Provider for a singleton of a String representing the current GCP instance ID. */
  @Provides
  @GcpInstanceId
  @Singleton
  String provideInstanceId(
      @GcpInstanceIdOverride String instanceIdOverride,
      @GcpClientConfigMetadataServiceClient GcpMetadataServiceClient gcpMetadataServiceClient)
      throws IOException {
    if (instanceIdOverride != null && !instanceIdOverride.isEmpty()) {
      return instanceIdOverride;
    } else {
      return gcpMetadataServiceClient.getGcpInstanceId();
    }
  }

  /** Provider for a singleton of a String representing the current GCP instance name. */
  @Provides
  @GcpInstanceName
  @Singleton
  String provideInstanceName(
      @GcpInstanceNameOverride String instanceNameOverride,
      @GcpClientConfigMetadataServiceClient GcpMetadataServiceClient gcpMetadataServiceClient)
      throws IOException {
    if (instanceNameOverride != null && !instanceNameOverride.isEmpty()) {
      return instanceNameOverride;
    } else {
      return gcpMetadataServiceClient.getGcpInstanceName();
    }
  }

  /** Provider for a singleton of a String representing the current GCP zone. */
  @Provides
  @GcpZone
  @Singleton
  String provideZone(
      @GcpZoneOverride String zoneOverride,
      @GcpClientConfigMetadataServiceClient GcpMetadataServiceClient gcpMetadataServiceClient)
      throws IOException {
    if (zoneOverride != null && !zoneOverride.isEmpty()) {
      return zoneOverride;
    } else {
      return gcpMetadataServiceClient.getGcpZone();
    }
  }

  /** Provider for a client config {@link GcpMetadataServiceClient}. */
  @Provides
  @GcpClientConfigMetadataServiceClient
  GcpMetadataServiceClient provideGcpMetadataServiceClient(
      @GcpClientConfigHttpClient HttpClient httpClient) {
    return new GcpMetadataServiceClient(httpClient);
  }

  /** Provider for a singleton of the {@code HttpClient} class. */
  @Provides
  @GcpClientConfigHttpClient
  @Singleton
  HttpClient provideHttpClient() {
    return HttpClients.createDefault();
  }
}
