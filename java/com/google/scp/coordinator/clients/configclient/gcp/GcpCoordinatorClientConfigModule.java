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
