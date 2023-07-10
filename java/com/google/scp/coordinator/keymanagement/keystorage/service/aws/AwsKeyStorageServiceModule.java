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

package com.google.scp.coordinator.keymanagement.keystorage.service.aws;

import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeyPublicKeySign;
import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeyPublicKeyVerify;
import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeySignatureAlgorithm;
import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeySignatureKeyId;
import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeySignatureKey;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbRegion;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;

import com.google.crypto.tink.Aead;
import com.google.crypto.tink.PublicKeySign;
import com.google.crypto.tink.PublicKeyVerify;
import com.google.crypto.tink.integration.awskmsv2.AwsKmsPublicKeySign;
import com.google.crypto.tink.integration.awskmsv2.AwsKmsPublicKeyVerify;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.aws.AwsCreateKeyTask;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.aws.AwsSignDataKeyTask;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.CoordinatorKekUri;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.CoordinatorKeyAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeySignatureAlgorithm;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeySignatureKeyId;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeySignatureAlgorithm;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeySignatureKeyId;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.KmsKeyAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.WorkerKekUri;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.CreateKeyTask;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.SignDataKeyTask;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDbModule;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.crypto.tink.aws.AwsTinkUtil;
import java.net.URI;
import java.security.GeneralSecurityException;
import java.time.Clock;
import java.util.Map;
import java.util.Optional;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.EnvironmentVariableCredentialsProvider;
import software.amazon.awssdk.http.SdkHttpClient;
import software.amazon.awssdk.http.urlconnection.UrlConnectionHttpClient;
import software.amazon.awssdk.services.kms.KmsClient;

/**
 * Defines dependencies for AWS implementation of KeyStorageService. It is intended to be used with
 * the CreateKeyApiGatewayHandler.
 */
public final class AwsKeyStorageServiceModule extends AbstractModule {

  /**
   * Environment variable for the the KEK URI (e.g. aws-kms://arn...) of key used for encrypting
   * DataKeys sent to Coordinator A.
   */
  private static final String COORDINATOR_KEK_URI_ENV_VAR = "COORDINATOR_KEK_URI";
  /** Environment variable for specifying {@link DataKeySignatureKeyId}. */
  private static final String DATA_KEY_SIGNATURE_KEY_ID = "DATA_KEY_SIGNATURE_KEY_ID";

  private static final String DATA_KEY_SIGNATURE_ALGORITHM = "DATA_KEY_SIGNATURE_ALGORITHM";
  /** Environment variable for the table name. */
  private static final String KEYSTORE_TABLE_ENV_VAR = "KEYSTORE_TABLE_NAME";
  /** Environment variable for AWS region */
  private static final String AWS_REGION = "AWS_REGION";
  /** Environment variable for AWS KMS URI */
  private static final String AWS_KMS_URI = "AWS_KMS_URI";
  /** Environment variable containing endpoint override for DDB. Used for testing. */
  private static final String DDB_OVERRIDE_ENV_VAR = "DDB_ENDPOINT_OVERRIDE";
  /** Environment variable containing endpoint override for KMS. Used for testing. */
  private static final String KMS_OVERRIDE_ENV_VAR = "KMS_ENDPOINT_OVERRIDE";
  /**
   * Environment variable for specifying signature key for signing public key material. If empty,
   * does not sign.
   */
  private static final String ENCRYPTION_KEY_SIGNATURE_KEY_ID = "ENCRYPTION_KEY_SIGNATURE_KEY_ID";
  /**
   * Environment variable for specifying algorithm of ENCRYPTION_KEY_SIGNATURE_KEY_ID. Must be
   * present if ENCRYPTION_KEY_SIGNATURE_KEY_ID is not empty
   */
  private static final String ENCRYPTION_KEY_SIGNATURE_ALGORITHM =
      "ENCRYPTION_KEY_SIGNATURE_ALGORITHM";

  private static final Map<String, String> env = System.getenv();

  private String getDataKeySignatureKeyId() {
    return env.getOrDefault(DATA_KEY_SIGNATURE_KEY_ID, "unknown_kms_uri");
  }

  private String getDataKeySignatureAlgorithm() {
    return env.getOrDefault(DATA_KEY_SIGNATURE_ALGORITHM, "unknown_kms_algorithm");
  }

  private String getCoordinatorKeyUri() {
    return env.getOrDefault(COORDINATOR_KEK_URI_ENV_VAR, "unknown_kms_uri");
  }

  private String getWorkerKeyUri() {
    return env.getOrDefault(AWS_KMS_URI, "unknown_kms_uri");
  }

  /** Returns name of DynamoDb table for storing keys. */
  public String getDynamoKeyDbTableName() {
    return env.getOrDefault(KEYSTORE_TABLE_ENV_VAR, "unspecified_table");
  }

  private Optional<String> getEncryptionKeySignatureKeyId() {
    return Optional.ofNullable(env.get(ENCRYPTION_KEY_SIGNATURE_KEY_ID))
        .filter(uri -> !uri.isEmpty());
  }

  private String getEncryptionKeySignatureAlgorithm() {
    return env.getOrDefault(ENCRYPTION_KEY_SIGNATURE_ALGORITHM, "unknown_kms_algorithm");
  }

  @Provides
  @Singleton
  @KmsKeyAead
  public Aead getAead(
      @WorkerKekUri String keyEncryptionKeyUri, CloudAeadSelector cloudAeadSelector) {
    try {
      return cloudAeadSelector.getAead(keyEncryptionKeyUri);
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Error getting Aead.", e);
    }
  }

  @Provides
  @Singleton
  @CoordinatorKeyAead
  public Aead getSymmetricKeyAead(
      @CoordinatorKekUri String uri, CloudAeadSelector cloudAeadSelector) {
    try {
      return cloudAeadSelector.getAead(uri);
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Error getting Aead.", e);
    }
  }

  @Provides
  public CloudAeadSelector provideCloudAeadSelector(
      AwsCredentialsProvider credentialsProvider, SdkHttpClient httpClient) {
    return AwsTinkUtil.getKmsAeadSelector(
        credentialsProvider, httpClient, getKmsEndpointOverride());
  }

  @Provides
  public KmsClient provideKmsClient(
      AwsCredentialsProvider credentialsProvider, SdkHttpClient httpClient) {
    return KmsClient.builder()
        .credentialsProvider(credentialsProvider)
        .httpClient(httpClient)
        .applyMutation(
            builder -> {
              getKmsEndpointOverride().ifPresent(builder::endpointOverride);
            })
        .build();
  }

  @Provides
  @Singleton
  @DataKeyPublicKeySign
  public PublicKeySign providePublicKeySign(
      KmsClient kmsClient,
      @DataKeySignatureKeyId String signatureKeyId,
      @DataKeySignatureAlgorithm String signatureAlgorithm) {
    return new AwsKmsPublicKeySign(kmsClient, signatureKeyId, signatureAlgorithm);
  }

  @Provides
  @Singleton
  @DataKeyPublicKeyVerify
  public PublicKeyVerify provideDataKeyPublicKeyVerify(
      KmsClient kmsClient,
      @DataKeySignatureKeyId String signatureKeyId,
      @DataKeySignatureAlgorithm String signatureAlgorithm) {
    return new AwsKmsPublicKeyVerify(kmsClient, signatureKeyId, signatureAlgorithm);
  }

  @Provides
  @Singleton
  @EncryptionKeySignatureKeyId
  public Optional<String> providesEncryptionKeySignatureKeyId() {
    return getEncryptionKeySignatureKeyId();
  }

  @Provides
  @Singleton
  @EncryptionKeySignatureKey
  public Optional<PublicKeySign> provideEncryptionKeySignatureKeyPublicKeySign(
      KmsClient kmsClient,
      @EncryptionKeySignatureKeyId Optional<String> signatureKeyId,
      @EncryptionKeySignatureAlgorithm String signatureAlgorithm) {
    return signatureKeyId.map(
        keyId -> new AwsKmsPublicKeySign(kmsClient, keyId, signatureAlgorithm));
  }

  @Override
  protected void configure() {
    bind(SdkHttpClient.class).toInstance(UrlConnectionHttpClient.builder().build());
    // Lambda performance optimization: define explicit credential provider
    bind(AwsCredentialsProvider.class).toInstance(EnvironmentVariableCredentialsProvider.create());
    bind(String.class)
        .annotatedWith(DynamoKeyDbTableName.class)
        .toInstance(getDynamoKeyDbTableName());
    bind(String.class).annotatedWith(DynamoKeyDbRegion.class).toInstance(env.get(AWS_REGION));
    install(new DynamoKeyDbModule(Optional.ofNullable(env.get(DDB_OVERRIDE_ENV_VAR))));
    bind(KeyDb.class).to(DynamoKeyDb.class);
    bind(CreateKeyTask.class).to(AwsCreateKeyTask.class);
    bind(SignDataKeyTask.class).to(AwsSignDataKeyTask.class);
    bind(String.class).annotatedWith(CoordinatorKekUri.class).toInstance(getCoordinatorKeyUri());
    bind(String.class)
        .annotatedWith(DataKeySignatureKeyId.class)
        .toInstance(getDataKeySignatureKeyId());
    bind(String.class)
        .annotatedWith(DataKeySignatureAlgorithm.class)
        .toInstance(getDataKeySignatureAlgorithm());
    bind(String.class).annotatedWith(WorkerKekUri.class).toInstance(getWorkerKeyUri());
    bind(String.class)
        .annotatedWith(EncryptionKeySignatureAlgorithm.class)
        .toInstance(getEncryptionKeySignatureAlgorithm());
    bind(Clock.class).toInstance(Clock.systemDefaultZone());
  }

  private static Optional<URI> getKmsEndpointOverride() {
    return Optional.ofNullable(env.get(KMS_OVERRIDE_ENV_VAR)).map(URI::create);
  }
}
