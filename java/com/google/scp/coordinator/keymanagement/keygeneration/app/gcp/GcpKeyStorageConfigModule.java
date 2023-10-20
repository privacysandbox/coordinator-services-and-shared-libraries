package com.google.scp.coordinator.keymanagement.keygeneration.app.gcp;

import com.google.inject.AbstractModule;
import com.google.inject.Key;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.CoordinatorBHttpClient;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.GetDataKeyBaseUrlOverride;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyStorageServiceBaseUrl;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyStorageServiceCloudfunctionUrl;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.HttpKeyStorageClient;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.KeyStorageClient;
import com.google.scp.shared.clients.DefaultHttpClientRetryStrategy;
import com.google.scp.shared.gcp.util.GcpHttpInterceptorUtil;
import java.util.Optional;
import org.apache.http.client.HttpClient;
import org.apache.http.impl.client.HttpClients;

/** Module for Key Storage HTTP clients for GCP. */
public final class GcpKeyStorageConfigModule extends AbstractModule {

  /** Provides a singleton of the {@code HttpClient} class. */
  @Provides
  @Singleton
  @CoordinatorBHttpClient
  public HttpClient provideCoordinatorBHttpClient(
      @KeyStorageServiceBaseUrl String keyStorageServiceBaseUrl,
      @KeyStorageServiceCloudfunctionUrl Optional<String> keyStorageServiceCloudfunctionUrl) {
    return HttpClients.custom()
        .addInterceptorFirst(
            GcpHttpInterceptorUtil.createHttpInterceptor(
                keyStorageServiceCloudfunctionUrl.orElse(keyStorageServiceBaseUrl)))
        .setServiceUnavailableRetryStrategy(DefaultHttpClientRetryStrategy.getInstance())
        .build();
  }

  @Override
  protected void configure() {
    bind(new Key<Optional<String>>(GetDataKeyBaseUrlOverride.class) {})
        .toInstance(Optional.empty());
    bind(KeyStorageClient.class).to(HttpKeyStorageClient.class);
  }
}
