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

import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.AWS_KEY_SYNC_ENABLED;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.AWS_KEY_SYNC_KEYDB_REGION;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.AWS_KEY_SYNC_KEYDB_TABLE_NAME;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.AWS_KEY_SYNC_KMS_KEY_URI;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.AWS_KEY_SYNC_ROLE_ARN;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.AWS_KMS_KEY_URI;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.AWS_KMS_ROLE_ARN;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.AWS_XC_ENABLED;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KEYS_VALIDITY_IN_DAYS;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KEY_DB_NAME;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KEY_ID_TYPE;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KEY_STORAGE_SERVICE_BASE_URL;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KEY_STORAGE_SERVICE_CLOUDFUNCTION_URL;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KEY_TTL_IN_DAYS;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KMS_KEY_URI;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.NUMBER_OF_KEYS_TO_CREATE;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.PEER_COORDINATOR_KMS_KEY_URI;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.PEER_COORDINATOR_SERVICE_ACCOUNT;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.PEER_COORDINATOR_WIP_PROVIDER;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.SPANNER_INSTANCE;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.SUBSCRIPTION_ID;

import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.clients.configclient.gcp.GcpCoordinatorClientConfigModule;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationKeyCount;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationTtlInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationValidityInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyIdTypeName;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyStorageServiceBaseUrl;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyStorageServiceCloudfunctionUrl;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KmsKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.PeerCoordinatorKmsKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.PeerCoordinatorServiceAccount;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.PeerCoordinatorWipProvider;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener.Annotations.SubscriptionId;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener.CreateSplitKeysPubSubListener;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener.PubSubListener;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener.PubSubListenerConfig;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.KeyIdFactory;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.KeyIdType;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKeySyncEnabled;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKeySyncKmsKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKeySyncRoleArn;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKmsKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsKmsRoleArn;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.AwsXcEnabled;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.GcpSplitKeyGenerationTasksModule;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbRegion;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDbConfig;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDbModule;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpInstanceIdOverride;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpInstanceNameOverride;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpProjectId;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpProjectIdOverride;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpZoneOverride;
import com.google.scp.shared.clients.configclient.gcp.DefaultParameterModule;
import com.google.scp.shared.clients.configclient.gcp.GcpParameterModule;
import com.google.scp.shared.clients.configclient.model.ErrorReason;
import java.util.Optional;

/** Module for Key Generation Application. */
public final class KeyGenerationModule extends AbstractModule {

  private final KeyGenerationArgs args;

  public KeyGenerationModule(KeyGenerationArgs args) {
    this.args = args;
  }

  @Provides
  @Singleton
  @SubscriptionId
  String provideSubscriptionId(ParameterClient parameterClient) throws ParameterClientException {
    return parameterClient.getParameter(SUBSCRIPTION_ID).orElse(args.getSubscriptionId());
  }

  @Provides
  @Singleton
  @KeyGenerationKeyCount
  Integer provideKeyGenerationKeyCount(ParameterClient parameterClient)
      throws ParameterClientException {
    return parameterClient
        .getParameter(NUMBER_OF_KEYS_TO_CREATE)
        .map(Integer::valueOf)
        .orElseGet(args::getNumberOfKeysToCreate);
  }

  @Provides
  @Singleton
  @KeyGenerationValidityInDays
  Integer provideKeyGenerationValidityInDays(ParameterClient parameterClient)
      throws ParameterClientException {
    return parameterClient
        .getParameter(KEYS_VALIDITY_IN_DAYS)
        .map(Integer::valueOf)
        .orElseGet(args::getKeysValidityInDays);
  }

  @Provides
  @Singleton
  @KeyGenerationTtlInDays
  Integer provideKeyGenerationTtlInDays(ParameterClient parameterClient)
      throws ParameterClientException {
    return parameterClient
        .getParameter(KEY_TTL_IN_DAYS)
        .map(Integer::valueOf)
        .orElseGet(args::getTtlInDays);
  }

  @Provides
  @Singleton
  SpannerKeyDbConfig providerSpannerKeyDbConfig(
      @GcpProjectId String projectId, ParameterClient parameterClient)
      throws ParameterClientException {
    String spannerInstanceIdParam =
        parameterClient.getParameter(SPANNER_INSTANCE).orElse(args.getSpannerInstanceId());
    String spannerDbNameParam =
        parameterClient.getParameter(KEY_DB_NAME).orElse(args.getDatabaseId());

    return SpannerKeyDbConfig.builder()
        .setGcpProjectId(projectId)
        .setSpannerInstanceId(spannerInstanceIdParam)
        .setSpannerDbName(spannerDbNameParam)
        .setReadStalenessSeconds(15)
        .setEndpointUrl(args.getSpannerEndpoint())
        .build();
  }

  @Provides
  @Singleton
  PubSubListenerConfig providesPubSubListenerConfig() {
    return PubSubListenerConfig.newBuilder().setEndpointUrl(args.getPubSubEndpoint()).build();
  }

  @Provides
  @Singleton
  @KeyStorageServiceCloudfunctionUrl
  Optional<String> providesKeyStorageServiceCloudfunctionUrl(ParameterClient parameterClient)
      throws ParameterClientException {
    return Optional.ofNullable(parameterClient.getParameter(KEY_STORAGE_SERVICE_CLOUDFUNCTION_URL))
        .orElse(args.getKeyStorageServiceCloudfunctionUrl());
  }

  @Provides
  @Singleton
  @PeerCoordinatorKmsKeyUri
  String providesPeerCoordinatorKmsKeyUri(ParameterClient parameterClient)
      throws ParameterClientException {
    return parameterClient
        .getParameter(PEER_COORDINATOR_KMS_KEY_URI)
        .orElse(args.getPeerCoordinatorKmsKeyUri());
  }

  @Provides
  @Singleton
  @KmsKeyUri
  String providesKmsKeyUri(ParameterClient parameterClient) throws ParameterClientException {
    return parameterClient.getParameter(KMS_KEY_URI).orElse(args.getKmsKeyUri());
  }

  @Provides
  @Singleton
  @PeerCoordinatorServiceAccount
  String providesPeerCoordinatorServiceAccount(ParameterClient parameterClient)
      throws ParameterClientException {
    return parameterClient
        .getParameter(PEER_COORDINATOR_SERVICE_ACCOUNT)
        .orElse(args.getPeerCoordinatorServiceAccount());
  }

  @Provides
  @Singleton
  @PeerCoordinatorWipProvider
  String providesPeerCoordinatorWipProvider(ParameterClient parameterClient)
      throws ParameterClientException {
    return parameterClient
        .getParameter(PEER_COORDINATOR_WIP_PROVIDER)
        .orElse(args.getPeerCoordinatorWipProvider());
  }

  @Provides
  @Singleton
  @KeyStorageServiceBaseUrl
  String providesKeyStorageServiceBaseUrl(ParameterClient parameterClient)
      throws ParameterClientException {
    return parameterClient
        .getParameter(KEY_STORAGE_SERVICE_BASE_URL)
        .orElse(args.getKeyStorageServiceBaseUrl());
  }

  @Provides
  @KeyIdTypeName
  Optional<String> provideKeyIdTypeProvider(ParameterClient paramClient)
      throws ParameterClientException {
    return paramClient.getParameter(KEY_ID_TYPE).or(args::getKeyIdType);
  }

  @Provides
  @Singleton
  KeyIdFactory provideKeyIdFactoryProvider(@KeyIdTypeName Optional<String> keyIdType)
      throws ParameterClientException {
    return KeyIdType.fromString(keyIdType)
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "Key ID Type %s not found", ErrorReason.MISSING_REQUIRED_PARAMETER))
        .getKeyIdFactory();
  }

  @Provides
  @Singleton
  @AwsXcEnabled
  Boolean provideAwsXcEnabled(ParameterClient parameterClient) throws ParameterClientException {
    return getBooleanParameter(parameterClient, AWS_XC_ENABLED);
  }

  @Provides
  @Singleton
  @AwsKmsKeyUri
  Optional<String> provideAwsKmsUri(
      ParameterClient parameterClient, @AwsXcEnabled Boolean awsXcEnabled)
      throws ParameterClientException {
    return getParameterIf(parameterClient, AWS_KMS_KEY_URI, awsXcEnabled);
  }

  @Provides
  @Singleton
  @AwsKmsRoleArn
  Optional<String> provideAwsKmsRoleArn(
      ParameterClient parameterClient, @AwsXcEnabled Boolean awsXcEnabled)
      throws ParameterClientException {
    return getParameterIf(parameterClient, AWS_KMS_ROLE_ARN, awsXcEnabled);
  }

  @Provides
  @Singleton
  @AwsKeySyncEnabled
  Boolean provideAwsKeySyncEnabled(ParameterClient parameterClient)
      throws ParameterClientException {
    return getBooleanParameter(parameterClient, AWS_KEY_SYNC_ENABLED);
  }

  @Provides
  @Singleton
  @AwsKeySyncRoleArn
  Optional<String> provideAwsKeySyncRoleArn(
      ParameterClient parameterClient, @AwsKeySyncEnabled Boolean awsKeySyncEnabled)
      throws ParameterClientException {
    return getParameterIf(parameterClient, AWS_KEY_SYNC_ROLE_ARN, awsKeySyncEnabled);
  }

  @Provides
  @Singleton
  @AwsKeySyncKmsKeyUri
  Optional<String> provideAwsKeySyncKmsKeyUri(
      ParameterClient parameterClient, @AwsKeySyncEnabled Boolean awsKeySyncEnabled)
      throws ParameterClientException {
    return getParameterIf(parameterClient, AWS_KEY_SYNC_KMS_KEY_URI, awsKeySyncEnabled);
  }

  @Provides
  @Singleton
  @DynamoKeyDbRegion
  Optional<String> provideDynamoKeyDbRegion(
      ParameterClient parameterClient, @AwsKeySyncEnabled Boolean awsKeySyncEnabled)
      throws ParameterClientException {
    return getParameterIf(parameterClient, AWS_KEY_SYNC_KEYDB_REGION, awsKeySyncEnabled);
  }

  @Provides
  @Singleton
  @DynamoKeyDbTableName
  Optional<String> provideDynamoKeyDbTableName(
      ParameterClient parameterClient, @AwsKeySyncEnabled Boolean awsKeySyncEnabled)
      throws ParameterClientException {
    return getParameterIf(parameterClient, AWS_KEY_SYNC_KEYDB_TABLE_NAME, awsKeySyncEnabled);
  }

  private static Optional<String> getParameterIf(
      ParameterClient paramClient, String paramName, Boolean condition)
      throws ParameterClientException {
    if (condition) {
      Optional<String> parameter = paramClient.getParameter(paramName);
      if (parameter.isEmpty()) {
        throw new ParameterClientException(
            String.format("Unable to get required parameter %s", paramName),
            ErrorReason.MISSING_REQUIRED_PARAMETER);
      }
      return parameter;
    } else {
      return Optional.empty();
    }
  }

  private static Boolean getBooleanParameter(ParameterClient paramClient, String paramName)
      throws ParameterClientException {
    return paramClient.getParameter(paramName).map(s -> s.equalsIgnoreCase("true")).orElse(false);
  }

  @Override
  protected void configure() {
    bind(PubSubListener.class).to(CreateSplitKeysPubSubListener.class);

    // Key Storage Bindings
    install(new GcpKeyStorageConfigModule());

    install(
        new GcpSplitKeyGenerationTasksModule(
            args.getEncodedKeysetHandle(), args.getPeerCoordinatorEncodedKeysetHandle()));

    // Data layer bindings
    install(new SpannerKeyDbModule());

    // Client Bindings
    install(new GcpCoordinatorClientConfigModule());
    if (args.getUseDefaultParametersOnGcp()) {
      install(new DefaultParameterModule());
    } else {
      install(new GcpParameterModule());
    }
    bind(String.class).annotatedWith(GcpProjectIdOverride.class).toInstance(args.getProjectId());

    // Necessary but empty bindings
    bind(String.class).annotatedWith(GcpInstanceIdOverride.class).toInstance("");
    bind(String.class).annotatedWith(GcpZoneOverride.class).toInstance("");
    bind(String.class).annotatedWith(GcpInstanceNameOverride.class).toInstance("");
  }
}
