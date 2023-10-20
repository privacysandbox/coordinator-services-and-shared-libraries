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

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KmsClient;
import com.google.crypto.tink.integration.awskmsv2.AwsKmsV2Client;
import com.google.inject.BindingAnnotation;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyServiceImpl;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyServiceModule;
import com.google.scp.operator.cpio.cryptoclient.HttpPrivateKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.PrivateKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.aws.Annotations.KmsEndpointOverride;
import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsCredentialAccessKey;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsCredentialSecretKey;
import com.google.scp.shared.clients.configclient.model.ErrorReason;
import com.google.scp.shared.clients.configclient.model.WorkerParameter;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;
import java.net.URI;
import java.security.GeneralSecurityException;
import software.amazon.awssdk.auth.credentials.AwsBasicCredentials;
import software.amazon.awssdk.auth.credentials.StaticCredentialsProvider;
import software.amazon.awssdk.http.apache.ApacheHttpClient;

/**
 * Guice module for implementation of {@link DecryptionKeyService} which fetches private key from an
 * HTTP endpoint and decrypts the private key by directly talking to AWS KMS.
 *
 * <p>This module does not perform binary attestation and therefore will only work in development
 * environments.
 */
public final class AwsKmsDecryptionKeyServiceModule extends DecryptionKeyServiceModule {

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

  /** Provides a singleton of the {@code KmsClient} class. */
  @Provides
  @Singleton
  KmsClient provideKmsClient(
      @KmsEndpointOverride URI kmsEndpointOverride,
      @AwsCredentialAccessKey String accessKey,
      @AwsCredentialSecretKey String secretKey,
      AwsSessionCredentialsProvider sessionCredentialsProvider)
      throws GeneralSecurityException {
    // TODO: Streamline this. StaticCredentialsProvider implements AwsCredentialsProvider but
    //  StsAwsSessionCredentialsProvider extends.
    if (!accessKey.isEmpty() && !secretKey.isEmpty()) {
      // Doesn't use STS.
      return (new AwsKmsV2Client())
          .withHttpClient(ApacheHttpClient.builder().build())
          .withKmsEndpointOverride(kmsEndpointOverride)
          .withCredentialsProvider(
              StaticCredentialsProvider.create(AwsBasicCredentials.create(accessKey, secretKey)));
    } else {
      return (new AwsKmsV2Client())
          .withHttpClient(ApacheHttpClient.builder().build())
          .withKmsEndpointOverride(kmsEndpointOverride)
          .withCredentialsProvider(sessionCredentialsProvider);
    }
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
  Aead provideAead(@AwsKmsKeyArn String awsKmsKeyArn, KmsClient kmsClient)
      throws GeneralSecurityException {
    return kmsClient.getAead(awsKmsKeyArn);
  }

  /**
   * Injection token representing the AWS ARN of the KMS key used for decrypting private keys
   * fetched by this service.
   */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface AwsKmsKeyArn {}
}
