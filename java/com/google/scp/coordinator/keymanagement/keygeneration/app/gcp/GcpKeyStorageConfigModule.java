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
import java.io.IOException;
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
      @KeyStorageServiceCloudfunctionUrl Optional<String> keyStorageServiceCloudfunctionUrl)
      throws IOException {
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
