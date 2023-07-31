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
import com.google.scp.shared.api.util.HttpClientWrapper;
import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.crypto.tink.aws.AwsTinkUtil;

/**
 * Guice module for implementation of {@link DecryptionKeyService}.
 *
 * <p>This module should only be used within the context of an AWS Nitro Enclave. Outside of an
 * enclave {@link AwsKmsMultiPartyDecryptionKeyServiceModule} should be used instead.
 */
public final class AwsEnclaveMultiPartyDecryptionKeyServiceModule
    extends DecryptionKeyServiceModule {

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

  /** A service to provide an {@code Aead} for coordinator A. */
  @Provides
  @Singleton
  @CoordinatorAAead
  CloudAeadSelector provideCoordinatorAAead(
      @CoordinatorACredentialsProvider AwsSessionCredentialsProvider credentialsProvider) {
    return AwsTinkUtil.getEnclaveAeadSelector(credentialsProvider);
  }

  /** A service to provide an {@code Aead} for coordinator B. */
  @Provides
  @Singleton
  @CoordinatorBAead
  CloudAeadSelector provideCoordinatorBAead(
      @CoordinatorBCredentialsProvider AwsSessionCredentialsProvider credentialsProvider) {
    return AwsTinkUtil.getEnclaveAeadSelector(credentialsProvider);
  }
}
