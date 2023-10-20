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

package com.google.scp.operator.cpio.cryptoclient.gcp;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.integration.gcpkms.GcpKmsClient;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.configclient.gcp.Annotations.AttestedCredentials;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyServiceImpl;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyServiceModule;
import com.google.scp.operator.cpio.cryptoclient.HttpPrivateKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.HttpPrivateKeyFetchingService.PrivateKeyServiceBaseUrl;
import com.google.scp.operator.cpio.cryptoclient.PrivateKeyFetchingService;
import com.google.scp.shared.api.util.HttpClientWrapper;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import com.google.scp.shared.clients.configclient.model.WorkerParameter;
import com.google.scp.shared.gcp.util.GcpHttpInterceptorUtil;
import java.security.GeneralSecurityException;

/**
 * Guice module for implementation of {@link DecryptionKeyService} which fetches private key from an
 * HTTP endpoint and decrypts the private key using gcloud KMS.
 */
public final class GcpKmsDecryptionKeyServiceModule extends DecryptionKeyServiceModule {

  /** Caller is expected to bind {@link GcpKmsDecryptionKeyServiceConfig}. */
  public GcpKmsDecryptionKeyServiceModule() {}

  @Override
  public Class<? extends DecryptionKeyService> getDecryptionKeyServiceImplementation() {
    return DecryptionKeyServiceImpl.class;
  }

  /** Provides a singleton of the {@code HttpClient} class. */
  @Provides
  @Singleton
  public HttpClientWrapper provideHttpClient(
      @PrivateKeyServiceBaseUrl String privateKeyServiceBaseUrl,
      GcpKmsDecryptionKeyServiceConfig config) {
    return HttpClientWrapper.builder()
        .setInterceptor(
            GcpHttpInterceptorUtil.createHttpInterceptor(
                config.coordinatorCloudfunctionUrl().orElse(privateKeyServiceBaseUrl)))
        .build();
  }

  @Override
  public void configureModule() {
    bind(PrivateKeyFetchingService.class)
        .to(HttpPrivateKeyFetchingService.class)
        .in(Singleton.class);
  }

  /** Provides a singleton of the {@code Aead} class. */
  @Provides
  @Singleton
  Aead provideAead(
      GcpKmsDecryptionKeyServiceConfig config,
      @AttestedCredentials GoogleCredentials credentials,
      ParameterClient parameterClient)
      throws ParameterClientException {
    String kmsKeyUri =
        parameterClient
            .getParameter(WorkerParameter.COORDINATOR_KMS_ARN.name())
            .orElse(config.coordinatorAKmsKeyUri());
    GcpKmsClient client = new GcpKmsClient();
    try {
      client.withCredentials(credentials);
      return client.getAead(kmsKeyUri);
    } catch (GeneralSecurityException e) {
      throw new RuntimeException(
          String.format("Error getting gcloud Aead with uri %s.", config.coordinatorAKmsKeyUri()),
          e);
    }
  }
}
