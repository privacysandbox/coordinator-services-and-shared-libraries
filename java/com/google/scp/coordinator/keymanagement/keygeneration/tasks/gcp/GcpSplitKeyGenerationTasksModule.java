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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp;

import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KMS_KEY_URI;
import static com.google.scp.coordinator.keymanagement.shared.util.GcpAeadProvider.getAeadFromEncodedKeysetHandle;
import static com.google.scp.coordinator.keymanagement.shared.util.GcpAeadProvider.getAwsAead;
import static com.google.scp.coordinator.keymanagement.shared.util.GcpAeadProvider.getGcpAead;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.crypto.tink.Aead;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.clients.configclient.gcp.Annotations.PeerCoordinatorCredentials;
import com.google.scp.coordinator.clients.configclient.gcp.GcpCoordinatorClientConfig;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KmsKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.PeerCoordinatorKmsKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.PeerCoordinatorServiceAccount;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.PeerCoordinatorWipProvider;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KmsKeyAead;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateSplitKeyTask;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKeySyncEnabled;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKeySyncKeyAead;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKeySyncKeyDb;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKeySyncKeyDbClient;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKeySyncKmsKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKeySyncRoleArn;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKmsKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKmsRoleArn;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsXcEnabled;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.PeerCoordinatorKeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.PeerCoordinatorKmsKeyAead;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbRegion;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDb;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.shared.aws.credsprovider.FederatedAwsCredentialsProvider;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import java.util.Optional;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.http.SdkHttpClient;
import software.amazon.awssdk.http.apache.ApacheHttpClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClientBuilder;

/** Module for Business Layer bindings used for KeyGeneration. Handles multiparty key generation. */
public final class GcpSplitKeyGenerationTasksModule extends AbstractModule {
  private final Optional<String> encodedKeysetHandle;
  private final Optional<String> peerCoordinatorEncodedKeysetHandle;

  public GcpSplitKeyGenerationTasksModule(
      Optional<String> encodedKeysetHandle, Optional<String> peerCoordinatorEncodedKeysetHandle) {
    this.encodedKeysetHandle = encodedKeysetHandle;
    this.peerCoordinatorEncodedKeysetHandle = peerCoordinatorEncodedKeysetHandle;
  }

  /** Provides the kmsKeyUri for this coordinator from either args or parameter client. */
  @Provides
  @Singleton
  @KeyEncryptionKeyUri
  String provideKeyEncryptionKeyUri(
      ParameterClient parameterClient,
      @KmsKeyUri String kmsKeyUri,
      @AwsXcEnabled Boolean awsXcEnabled,
      @AwsKmsKeyUri Optional<String> awsKmsKeyUri)
      throws ParameterClientException {
    if (awsXcEnabled) {
      return awsKmsKeyUri.get();
    }
    return parameterClient.getParameter(KMS_KEY_URI).orElse(kmsKeyUri);
  }

  /** Provides a {@code KmsClient} for this coordinator. */
  @Provides
  @Singleton
  @KmsKeyAead
  Aead provideAead(
      @KeyEncryptionKeyUri String kmsKeyUri,
      @AwsXcEnabled Boolean awsXcEnabled,
      @AwsKmsKeyUri Optional<String> awsKmsKeyUri,
      @AwsKmsRoleArn Optional<String> awsKmsRoleArn) {
    if (awsXcEnabled) {
      return getAwsAead(awsKmsKeyUri.get(), awsKmsRoleArn.get());
    } else {
      return encodedKeysetHandle.isPresent()
          ? getAeadFromEncodedKeysetHandle(encodedKeysetHandle.get())
          : getGcpAead(kmsKeyUri, Optional.empty());
    }
  }

  /** Provides a {@code KmsClient} for peer coordinator. */
  @Provides
  @Singleton
  @PeerCoordinatorKmsKeyAead
  Aead providePeerCoordinatorAead(
      @PeerCoordinatorKeyEncryptionKeyUri String peerCoordinatorKmsKeyUri,
      @PeerCoordinatorCredentials GoogleCredentials credentials) {
    return peerCoordinatorEncodedKeysetHandle.isPresent()
        ? getAeadFromEncodedKeysetHandle(peerCoordinatorEncodedKeysetHandle.get())
        : getGcpAead(peerCoordinatorKmsKeyUri, Optional.of(credentials));
  }

  @Provides
  @Singleton
  @PeerCoordinatorKeyEncryptionKeyUri
  String providesPeerCoordinatorKeyEncryptionKeyUri(
      @PeerCoordinatorKmsKeyUri String peerCoordinatorKmsKeyUri) {
    return peerCoordinatorKmsKeyUri;
  }

  @Provides
  @Singleton
  GcpCoordinatorClientConfig providesGcpCoordinatorClientConfig(
      @PeerCoordinatorWipProvider String peerCoordinatorWipProvider,
      @PeerCoordinatorServiceAccount String peerCoordinatorServiceAccount) {
    GcpCoordinatorClientConfig.Builder configBuilder =
        GcpCoordinatorClientConfig.builder()
            .setPeerCoordinatorServiceAccount(peerCoordinatorServiceAccount)
            .setPeerCoordinatorWipProvider(peerCoordinatorWipProvider)
            .setUseLocalCredentials(peerCoordinatorWipProvider.isEmpty());
    return configBuilder.build();
  }

  @Provides
  @Singleton
  @AwsKeySyncKeyDbClient
  public Optional<DynamoDbEnhancedClient> provideAwsKeySyncKeyDbClient(
      @AwsKeySyncEnabled boolean awsKeySyncEnabled,
      @AwsKeySyncRoleArn Optional<String> roleArn,
      @DynamoKeyDbRegion Optional<String> region,
      SdkHttpClient httpClient) {
    if (!awsKeySyncEnabled) {
      return Optional.empty();
    }
    AwsCredentialsProvider credentials = new FederatedAwsCredentialsProvider(roleArn.get());
    DynamoDbClientBuilder builder =
        DynamoDbClient.builder()
            .credentialsProvider(credentials)
            .httpClient(httpClient)
            .region(Region.of(region.get()));
    return Optional.of(DynamoDbEnhancedClient.builder().dynamoDbClient(builder.build()).build());
  }

  @Provides
  @Singleton
  @AwsKeySyncKeyDb
  public Optional<KeyDb> provideAwsKeySyncKeyDb(
      @AwsKeySyncKeyDbClient Optional<DynamoDbEnhancedClient> keyDbClient,
      @DynamoKeyDbTableName Optional<String> tableName) {
    if (keyDbClient.isEmpty()) {
      return Optional.empty();
    }
    return Optional.of(new DynamoKeyDb(keyDbClient.get(), tableName.get()));
  }

  @Provides
  @Singleton
  @AwsKeySyncKeyAead
  public Optional<Aead> provideAwsKeySyncKeyAead(
      @AwsKeySyncEnabled boolean awsKeySyncEnabled,
      @AwsKeySyncKmsKeyUri Optional<String> awsKeySyncKmsKeyUri,
      @AwsKeySyncRoleArn Optional<String> awsKeySyncRoleArn) {
    if (!awsKeySyncEnabled) {
      return Optional.empty();
    }
    return Optional.of(getAwsAead(awsKeySyncKmsKeyUri.get(), awsKeySyncRoleArn.get()));
  }

  @Override
  protected void configure() {
    bind(CreateSplitKeyTask.class).to(GcpCreateSplitKeyTask.class);
    bind(SdkHttpClient.class).toInstance(ApacheHttpClient.builder().build());
  }
}
