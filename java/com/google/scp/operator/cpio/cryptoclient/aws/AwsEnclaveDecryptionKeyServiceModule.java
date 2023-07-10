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

import static com.google.scp.operator.cpio.configclient.Annotations.CoordinatorARegionBinding;
import static com.google.scp.operator.cpio.cryptoclient.aws.AwsKmsDecryptionKeyServiceModule.AwsKmsKeyArn;

import com.google.crypto.tink.Aead;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyServiceImpl;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyServiceModule;
import com.google.scp.operator.cpio.cryptoclient.HttpPrivateKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.PrivateKeyFetchingService;
import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import com.google.scp.shared.clients.configclient.model.ErrorReason;
import com.google.scp.shared.clients.configclient.model.WorkerParameter;
import com.google.scp.shared.crypto.tink.kmstoolenclave.KmsToolEnclaveAead;
import software.amazon.awssdk.regions.Region;

/**
 * Guice module for implementation of {@link DecryptionKeyService}.
 *
 * <p>This module should only be used within the context of an AWS Nitro Enclave. Outside of an
 * enclave {@link AwsKmsDecryptionKeyServiceModule} should be used instead.
 */
public final class AwsEnclaveDecryptionKeyServiceModule extends DecryptionKeyServiceModule {

  @Override
  public Class<? extends DecryptionKeyService> getDecryptionKeyServiceImplementation() {
    return DecryptionKeyServiceImpl.class;
  }

  @Override
  public void configureModule() {
    bind(PrivateKeyFetchingService.class)
        .to(HttpPrivateKeyFetchingService.class)
        .in(Singleton.class);
  }

  /** Provides the ARN for the AWS KMS key as a String. */
  @Provides
  @AwsKmsKeyArn
  String provideAwsKmsKeyArn(ParameterClient paramClient) throws ParameterClientException {
    return paramClient
        .getParameter(WorkerParameter.COORDINATOR_KMS_ARN.name())
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "Could not get KMS key ARN from the parameter client.",
                    ErrorReason.MISSING_REQUIRED_PARAMETER));
  }

  /** Provides an instance of the {@code Aead} class. */
  @Provides
  Aead provideAead(
      AwsSessionCredentialsProvider credentialsProvider, @CoordinatorARegionBinding Region region) {
    return new KmsToolEnclaveAead(credentialsProvider, region);
  }
}
