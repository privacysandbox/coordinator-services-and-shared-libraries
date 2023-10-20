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

package com.google.scp.operator.cpio.cryptoclient.aws;

import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorAHttpClient;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBHttpClient;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorACredentialsProvider;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorBCredentialsProvider;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorAAead;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorAEncryptionKeyServiceBaseUrl;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorBAead;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorBEncryptionKeyServiceBaseUrl;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyServiceModule;
import com.google.scp.operator.cpio.cryptoclient.EncryptionKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.HttpEncryptionKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.MultiPartyDecryptionKeyServiceImpl;
import com.google.scp.operator.cpio.cryptoclient.MultiPartyDecryptionKeyServiceImpl.CoordinatorAEncryptionKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.MultiPartyDecryptionKeyServiceImpl.CoordinatorBEncryptionKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.aws.Annotations.KmsEndpointOverride;
import com.google.scp.shared.api.util.HttpClientWrapper;
import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsCredentialAccessKey;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsCredentialSecretKey;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.crypto.tink.aws.AwsTinkUtil;
import java.net.URI;
import java.security.GeneralSecurityException;
import java.util.Optional;
import software.amazon.awssdk.auth.credentials.AwsBasicCredentials;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.StaticCredentialsProvider;
import software.amazon.awssdk.http.apache.ApacheHttpClient;

/**
 * Guice module for implementation of {@link DecryptionKeyService} which fetches private key from an
 * HTTP endpoint and decrypts the private key by directly talking to AWS KMS.
 *
 * <p>This module does not perform binary attestation and therefore will only work in development
 * environments.
 */
public final class AwsKmsMultiPartyDecryptionKeyServiceModule extends DecryptionKeyServiceModule {

  @Override
  public Class<? extends DecryptionKeyService> getDecryptionKeyServiceImplementation() {
    return MultiPartyDecryptionKeyServiceImpl.class;
  }

  @Provides
  @CoordinatorAEncryptionKeyFetchingService
  @Singleton
  public EncryptionKeyFetchingService provideCoordinatorAEncryptionKeyFetchingService(
      @CoordinatorAHttpClient HttpClientWrapper httpClient,
      @CoordinatorAEncryptionKeyServiceBaseUrl String coordinatorAEncryptionKeyServiceBaseUrl) {
    return new HttpEncryptionKeyFetchingService(
        httpClient, coordinatorAEncryptionKeyServiceBaseUrl);
  }

  @Provides
  @CoordinatorBEncryptionKeyFetchingService
  @Singleton
  public EncryptionKeyFetchingService provideCoordinatorBEncryptionKeyFetchingService(
      @CoordinatorBHttpClient HttpClientWrapper httpClient,
      @CoordinatorBEncryptionKeyServiceBaseUrl String coordinatorBEncryptionKeyServiceBaseUrl) {
    return new HttpEncryptionKeyFetchingService(
        httpClient, coordinatorBEncryptionKeyServiceBaseUrl);
  }

  private static CloudAeadSelector createCloudAeadSelector(
      String accessKey,
      String secretKey,
      URI kmsEndpointOverride,
      AwsSessionCredentialsProvider sessionCredentialsProvider)
      throws GeneralSecurityException {
    var httpClient = ApacheHttpClient.builder().build();
    var endpointOverride =
        Optional.of(kmsEndpointOverride).filter(uri -> !uri.toString().isEmpty());

    AwsCredentialsProvider credentialsProvider = sessionCredentialsProvider;

    if (!accessKey.isEmpty() && !secretKey.isEmpty()) {
      // Doesn't use STS.
      credentialsProvider =
          StaticCredentialsProvider.create(AwsBasicCredentials.create(accessKey, secretKey));
    }

    return AwsTinkUtil.getKmsAeadSelector(credentialsProvider, httpClient, endpointOverride);
  }

  /** Provides a {@code KmsClient} for coordinator A. */
  @Provides
  @Singleton
  @CoordinatorAAead
  CloudAeadSelector provideCoordinatorAKmsClient(
      @KmsEndpointOverride URI kmsEndpointOverride,
      @AwsCredentialAccessKey String accessKey,
      @AwsCredentialSecretKey String secretKey,
      @CoordinatorACredentialsProvider AwsSessionCredentialsProvider sessionCredentialsProvider)
      throws GeneralSecurityException {
    return createCloudAeadSelector(
        accessKey, secretKey, kmsEndpointOverride, sessionCredentialsProvider);
  }

  /** Provides a {@code KmsClient} for coordinator B. */
  @Provides
  @Singleton
  @CoordinatorBAead
  CloudAeadSelector provideCoordinatorBKmsClient(
      @KmsEndpointOverride URI kmsEndpointOverride,
      @AwsCredentialAccessKey String accessKey,
      @AwsCredentialSecretKey String secretKey,
      @CoordinatorBCredentialsProvider AwsSessionCredentialsProvider sessionCredentialsProvider)
      throws GeneralSecurityException {
    return createCloudAeadSelector(
        accessKey, secretKey, kmsEndpointOverride, sessionCredentialsProvider);
  }
}
