/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.clients.configclient.gcp;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.clients.configclient.gcp.Annotations.PeerCoordinatorCredentials;
import com.google.scp.shared.clients.configclient.gcp.CredentialsHelper;
import com.google.scp.shared.clients.configclient.gcp.GcpClientConfigModule;
import java.io.IOException;

/** Provides necessary configurations for GCP config client. */
public final class GcpCoordinatorClientConfigModule extends AbstractModule {

  /** Caller is expected to bind {@link GcpCoordinatorClientConfig}. */
  public GcpCoordinatorClientConfigModule() {}

  /**
   * Provider for a singleton of the {@code GoogleCredentials} class which represents TEE attested
   * credentials for peer coordinator.
   */
  @Provides
  @Singleton
  @PeerCoordinatorCredentials
  GoogleCredentials providePeerCoordinatorCredentials(GcpCoordinatorClientConfig config)
      throws IOException {
    if (config.useLocalCredentials()) {
      return GoogleCredentials.getApplicationDefault();
    }
    return CredentialsHelper.getAttestedCredentials(
        config.peerCoordinatorWipProvider(), config.peerCoordinatorServiceAccount());
  }

  @Override
  protected void configure() {
    install(new GcpClientConfigModule());
  }
}
