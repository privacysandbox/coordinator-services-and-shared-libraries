package com.google.scp.shared.clients.configclient.gcp;

import com.google.cloud.secretmanager.v1.SecretManagerServiceClient;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterModule;
import java.io.IOException;

/** Provides necessary configurations for GCP parameter client. */
public final class GcpParameterModule extends ParameterModule {

  @Override
  public Class<? extends ParameterClient> getParameterClientImpl() {
    return GcpParameterClient.class;
  }

  @Provides
  @Singleton
  SecretManagerServiceClient provideSecretManagerServiceClient() throws IOException {
    return SecretManagerServiceClient.create();
  }
}
