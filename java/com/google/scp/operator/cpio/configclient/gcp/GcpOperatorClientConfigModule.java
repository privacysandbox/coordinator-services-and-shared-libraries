package com.google.scp.operator.cpio.configclient.gcp;

import static com.google.scp.operator.cpio.configclient.common.ConfigClientUtil.COORDINATOR_HTTPCLIENT_MAX_ATTEMPTS;
import static com.google.scp.operator.cpio.configclient.common.ConfigClientUtil.COORDINATOR_HTTPCLIENT_RETRY_INITIAL_INTERVAL;
import static com.google.scp.operator.cpio.configclient.common.ConfigClientUtil.COORDINATOR_HTTPCLIENT_RETRY_MULTIPLIER;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorAHttpClient;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBHttpClient;
import com.google.scp.operator.cpio.configclient.gcp.Annotations.AttestedCredentials;
import com.google.scp.operator.cpio.configclient.gcp.Annotations.CoordinatorACredentials;
import com.google.scp.operator.cpio.configclient.gcp.Annotations.CoordinatorBCredentials;
import com.google.scp.shared.api.util.HttpClientWrapper;
import com.google.scp.shared.clients.configclient.gcp.CredentialsHelper;
import com.google.scp.shared.clients.configclient.gcp.GcpClientConfigModule;
import com.google.scp.shared.clients.configclient.gcp.GcpOperatorClientConfig;
import com.google.scp.shared.gcp.util.GcpHttpInterceptorUtil;
import java.io.IOException;

/** Provides necessary configurations for GCP config client. */
public final class GcpOperatorClientConfigModule extends AbstractModule {

  /** Caller is expected to bind {@link GcpOperatorClientConfig}. */
  public GcpOperatorClientConfigModule() {}

  /**
   * Provider for a singleton of the {@code GoogleCredentials} class which represents TEE attested
   * credentials for CoordinatorA.
   */
  @Provides
  @AttestedCredentials
  @Singleton
  GoogleCredentials provideCredentials(@CoordinatorACredentials GoogleCredentials credentials) {
    return credentials;
  }

  /**
   * Provider for a singleton of the {@code GoogleCredentials} class which represents TEE attested
   * credentials for CoordinatorA.
   */
  @Provides
  @CoordinatorACredentials
  @Singleton
  GoogleCredentials provideCoordinatorACredentials(GcpOperatorClientConfig config)
      throws IOException {
    if (config.useLocalCredentials()) {
      return GoogleCredentials.getApplicationDefault();
    }
    return CredentialsHelper.getAttestedCredentials(
        config.coordinatorAWipProvider(), config.coordinatorAServiceAccountToImpersonate());
  }

  /**
   * Provider for a singleton of the {@code GoogleCredentials} class which represents TEE attested
   * credentials for CoordinatorB.
   */
  @Provides
  @CoordinatorBCredentials
  @Singleton
  GoogleCredentials provideCoordinatorBCredentials(GcpOperatorClientConfig config)
      throws IOException {
    // For single party solution, we will not have coordinatorB details, so use default
    // credentials. These are not used for single party solution.
    if (config.useLocalCredentials() || config.coordinatorBWipProvider().isEmpty()) {
      return GoogleCredentials.getApplicationDefault();
    }
    return CredentialsHelper.getAttestedCredentials(
        config.coordinatorBWipProvider().get(),
        config.coordinatorBServiceAccountToImpersonate().get());
  }

  /** Provides a singleton of the {@code HttpClientWrapper} class. */
  @Provides
  @CoordinatorAHttpClient
  @Singleton
  public HttpClientWrapper provideCoordinatorAHttpClient(GcpOperatorClientConfig config) {
    return getHttpClientWrapper(
        config
            .coordinatorAEncryptionKeyServiceCloudfunctionUrl()
            .orElse(config.coordinatorAEncryptionKeyServiceBaseUrl()));
  }

  /** Provides a singleton of the {@code HttpClientWrapper} class. */
  @Provides
  @CoordinatorBHttpClient
  @Singleton
  public HttpClientWrapper provideCoordinatorBHttpClient(GcpOperatorClientConfig config) {
    if (config.coordinatorBEncryptionKeyServiceBaseUrl().isPresent()) {
      return getHttpClientWrapper(
          config
              .coordinatorBEncryptionKeyServiceCloudfunctionUrl()
              .orElse(config.coordinatorBEncryptionKeyServiceBaseUrl().get()));
    }
    return HttpClientWrapper.createDefault();
  }

  @Override
  protected void configure() {
    install(new GcpClientConfigModule());
  }

  private static HttpClientWrapper getHttpClientWrapper(String url) {
    return HttpClientWrapper.builder()
        .setInterceptor(GcpHttpInterceptorUtil.createHttpInterceptor(url))
        .setExponentialBackoff(
            COORDINATOR_HTTPCLIENT_RETRY_INITIAL_INTERVAL,
            COORDINATOR_HTTPCLIENT_RETRY_MULTIPLIER,
            COORDINATOR_HTTPCLIENT_MAX_ATTEMPTS)
        .build();
  }
}
