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

package com.google.scp.shared.testutils.crypto;

import com.google.acai.TestScoped;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KmsClient;
import com.google.crypto.tink.integration.awskmsv2.AwsKmsV2Client;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.operator.cpio.cryptoclient.PrivateKeyFetchingService;
import com.google.scp.operator.cpio.cryptoclient.aws.AwsKmsDecryptionKeyServiceModule.AwsKmsKeyArn;
import com.google.scp.operator.cpio.cryptoclient.testing.FakePrivateKeyFetchingService;
import com.google.scp.shared.testutils.aws.AwsHermeticTestHelper;
import java.security.GeneralSecurityException;
import org.testcontainers.containers.localstack.LocalStackContainer;
import software.amazon.awssdk.auth.credentials.AwsBasicCredentials;
import software.amazon.awssdk.auth.credentials.StaticCredentialsProvider;
import software.amazon.awssdk.http.apache.ApacheHttpClient;

/**
 * Provides a {@link DecryptionKeyServiceImpl} configured for use with AWS KMS.
 *
 * <p>Can be used for both a Localstack version of KMS and prod AWS KMS.
 */
public class AwsTinkUtilsModule extends AbstractModule {
  private final KmsClient kmsClient;
  private final String keyArn;

  private AwsTinkUtilsModule(KmsClient kmsClient, String keyArn) {
    this.kmsClient = kmsClient;
    this.keyArn = keyArn;
  }

  /**
   * Creates a new AwsTinkUtilsModule configured to use the provided localstack container as its KMS
   * server rather than AWS KMS.
   *
   * <p>Creates the necessary symmetric encryption key in localstack.
   */
  public static AwsTinkUtilsModule withLocalstack(LocalStackContainer localstack) {
    var keyId = AwsHermeticTestHelper.createKey(AwsHermeticTestHelper.createKmsClient(localstack));
    var keyArn = String.format("aws-kms://arn:aws:kms:us-east-1:000000000000:key/%s", keyId);
    KmsClient kmsClient;
    try {
      kmsClient =
          new AwsKmsV2Client()
              .withHttpClient(ApacheHttpClient.create())
              .withKmsEndpointOverride(
                  localstack.getEndpointOverride(LocalStackContainer.Service.KMS))
              .withCredentialsProvider(
                  StaticCredentialsProvider.create(
                      AwsBasicCredentials.create(
                          localstack.getAccessKey(), localstack.getSecretKey())));
    } catch (GeneralSecurityException e) {
      throw new IllegalStateException("Failed to create KMS client", e);
    }

    return new AwsTinkUtilsModule(kmsClient, keyArn);
  }

  /**
   * Creates a new AwsTinkUtilsModule which uses prod AWS KMS for its key encryption and decryption
   * and uses the default credentials provider.
   *
   * @param keyArn The ARN of the KMS symmetric key to use for encryption and decryption. Can be any
   *     key for which the default credentials on the system have ENCRYPT and DECRYPT permissions.
   */
  public static AwsTinkUtilsModule withRealKey(String keyArn) {
    return new AwsTinkUtilsModule(new AwsKmsV2Client(), String.format("aws-kms://%s", keyArn));
  }

  @Provides
  TinkUtils provideTinkUtils(Aead aead, FakePrivateKeyFetchingService keyFetchingService)
      throws GeneralSecurityException {
    return new TinkUtils(aead, keyFetchingService);
  }

  @Provides
  Aead provideAead(@AwsKmsKeyArn String awsKmsKeyArn, KmsClient kmsClient)
      throws GeneralSecurityException {
    return kmsClient.getAead(awsKmsKeyArn);
  }

  @Override
  protected void configure() {
    bind(FakePrivateKeyFetchingService.class).in(TestScoped.class);
    bind(PrivateKeyFetchingService.class).to(FakePrivateKeyFetchingService.class);
    bind(KmsClient.class).toInstance(kmsClient);
    bind(String.class).annotatedWith(AwsKmsKeyArn.class).toInstance(keyArn);
  }
}
