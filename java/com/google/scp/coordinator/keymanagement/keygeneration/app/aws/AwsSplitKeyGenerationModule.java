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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws;

import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationQueueMessageLeaseSeconds;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationSqsEndpointOverrideBinding;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationSqsMaxWaitTimeSeconds;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationSqsUrl;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.GetDataKeyBaseUrlOverride;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationKeyCount;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationTtlInDays;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationValidityInDays;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyStorageServiceBaseUrl;
import static com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.EncryptionKeySignatureKey;
import static com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KeyEncryptionKeyUri;
import static com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KmsKeyAead;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbRegion;
import static com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbTableName;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.COORDINATOR_B_ASSUME_ROLE_ARN;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.ENCRYPTION_KEY_SIGNATURE_ALGORITHM;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.ENCRYPTION_KEY_SIGNATURE_KEY_ID;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KEY_DB_NAME;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KEY_DB_REGION;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KEY_GENERATION_QUEUE_URL;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KEY_ID_TYPE;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KEY_STORAGE_SERVICE_BASE_URL;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyGenerationParameter.KMS_KEY_URI;
import static com.google.scp.shared.clients.configclient.Annotations.ApplicationRegionBinding;
import static com.google.scp.shared.clients.configclient.Annotations.ApplicationRegionBindingOverride;
import static com.google.scp.shared.clients.configclient.aws.AwsParameterModule.Ec2EndpointOverrideBinding;
import static com.google.scp.shared.clients.configclient.aws.AwsParameterModule.SsmEndpointOverrideBinding;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.common.util.concurrent.ServiceManager;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.PublicKeySign;
import com.google.crypto.tink.integration.awskmsv2.AwsKmsPublicKeySign;
import com.google.crypto.tink.integration.awskmsv2.AwsKmsV2Client;
import com.google.inject.AbstractModule;
import com.google.inject.BindingAnnotation;
import com.google.inject.Key;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.CoordinatorBCredentialsProvider;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.CoordinatorBHttpClient;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyIdTypeName;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.HttpKeyStorageClient;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.KeyStorageClient;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.aws.AwsCreateSplitKeyTask;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateSplitKeyTask;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.KeyIdFactory;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.KeyIdType;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.DynamoKeyDbModule;
import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import com.google.scp.shared.aws.credsprovider.SelfAwsSessionCredentialsProvider;
import com.google.scp.shared.aws.credsprovider.StsAwsSessionCredentialsProvider;
import com.google.scp.shared.aws.util.AwsRequestSigner;
import com.google.scp.shared.clients.DefaultHttpClientRetryStrategy;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsCredentialAccessKey;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsCredentialSecretKey;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsEc2MetadataEndpointOverride;
import com.google.scp.shared.clients.configclient.local.Annotations.ParameterValues;
import com.google.scp.shared.clients.configclient.model.ErrorReason;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.crypto.tink.aws.AwsTinkUtil;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.net.URI;
import java.security.GeneralSecurityException;
import java.util.NoSuchElementException;
import java.util.Optional;
import org.apache.http.client.HttpClient;
import org.apache.http.impl.client.HttpClients;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.core.client.config.ClientOverrideConfiguration;
import software.amazon.awssdk.core.retry.RetryPolicy;
import software.amazon.awssdk.http.SdkHttpClient;
import software.amazon.awssdk.http.apache.ApacheHttpClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.kms.KmsClient;
import software.amazon.awssdk.services.sqs.SqsClient;
import software.amazon.awssdk.services.sqs.SqsClientBuilder;
import software.amazon.awssdk.services.sts.StsClient;

/** Module for Key Generation Application, generating key splits. */
public final class AwsSplitKeyGenerationModule extends AbstractModule {
  private final AwsSplitKeyGenerationArgs args;

  public AwsSplitKeyGenerationModule(AwsSplitKeyGenerationArgs args) {
    this.args = args;
  }

  @Override
  protected void configure() {
    bind(CreateSplitKeyTask.class).to(AwsCreateSplitKeyTask.class);
    bind(SdkHttpClient.class).toInstance(ApacheHttpClient.builder().build());
    install(args.getParamClient().getParameterClientModule());
    install(
        new DynamoKeyDbModule(
            Optional.of(args.getKeyDbEndpointOverride())
                .filter(endpointOverride -> !endpointOverride.isEmpty())));
    bind(String.class)
        .annotatedWith(ApplicationRegionBindingOverride.class)
        .toInstance(args.getApplicationRegionOverride());
    bind(String.class).annotatedWith(AwsCredentialSecretKey.class).toInstance(args.getAccessKey());
    bind(String.class).annotatedWith(AwsCredentialAccessKey.class).toInstance(args.getSecretKey());
    bind(String.class)
        .annotatedWith(AwsEc2MetadataEndpointOverride.class)
        .toInstance(args.getAwsMetadataEndpointOverride());
    bind(Integer.class)
        .annotatedWith(KeyGenerationValidityInDays.class)
        .toInstance(args.getValidityInDays());
    bind(Integer.class).annotatedWith(KeyGenerationKeyCount.class).toInstance(args.getKeyCount());
    bind(Integer.class).annotatedWith(KeyGenerationTtlInDays.class).toInstance(args.getTtlInDays());
    install(new AwsClientConfigModule());
    bind(int.class)
        .annotatedWith(KeyGenerationSqsMaxWaitTimeSeconds.class)
        .toInstance(args.getKeyGenerationSqsMaxWaitTimeSeconds());
    bind(int.class)
        .annotatedWith(KeyGenerationQueueMessageLeaseSeconds.class)
        .toInstance(args.getKeyGenerationQueueMessageLeaseSeconds());
    bind(URI.class)
        .annotatedWith(Ec2EndpointOverrideBinding.class)
        .toInstance(args.getEc2EndpointOverride());
    bind(URI.class)
        .annotatedWith(KeyGenerationSqsEndpointOverrideBinding.class)
        .toInstance(args.getSqsEndpointOverride());
    bind(URI.class)
        .annotatedWith(SsmEndpointOverrideBinding.class)
        .toInstance(args.getSsmEndpointOverride());
    bind(URI.class)
        .annotatedWith(KmsEndpointOverride.class)
        .toInstance(args.getKmsEndpointOverride());

    bind(new Key<Optional<String>>(GetDataKeyBaseUrlOverride.class) {})
        .toInstance(args.getGetDataKeyOverrideBaseUrl());
    bind(KeyStorageClient.class).to(HttpKeyStorageClient.class);
  }

  @Provides
  @Singleton
  ServiceManager provideServiceManager(SqsKeyGenerationService sqsKeyGenerationService) {
    return new ServiceManager(ImmutableList.of(sqsKeyGenerationService));
  }

  // TODO(b/239625240): Use centralized annotation once available.
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  private @interface KmsEndpointOverride {}

  /** Only used if ParamClient is ARGS; otherwise values come from AWS parameter store */
  @Provides
  @ParameterValues
  public ImmutableMap<String, String> provideParameterValues() {
    var paramMap =
        ImmutableMap.<String, String>builder()
            .put(KEY_GENERATION_QUEUE_URL, args.getKeyGenerationSqsUrl())
            .put(KEY_STORAGE_SERVICE_BASE_URL, args.getKeyStorageServiceBaseUrl())
            .put(KMS_KEY_URI, args.getKmsKeyUri())
            .put(KEY_DB_NAME, args.getKeyDbName())
            .put(KEY_DB_REGION, args.getKeyDbRegion())
            .put(ENCRYPTION_KEY_SIGNATURE_ALGORITHM, args.getSignatureAlgorithm())
            .put(KEY_ID_TYPE, args.getKeyIdType().orElse(""));
    if (args.getCoordinatorBAssumeRoleArn().isPresent()) {
      paramMap.put(COORDINATOR_B_ASSUME_ROLE_ARN, args.getCoordinatorBAssumeRoleArn().get());
    }
    if (args.getSignatureKeyId().isPresent()) {
      paramMap.put(ENCRYPTION_KEY_SIGNATURE_KEY_ID, args.getSignatureKeyId().get());
    }
    return paramMap.build();
  }

  @Provides
  @Singleton
  @KmsKeyAead
  public Aead provideKeyEncryptionKeyAead(
      SdkHttpClient httpClient,
      AwsCredentialsProvider credentialsProvider,
      @KeyEncryptionKeyUri String keyEncryptionKeyUri,
      @KmsEndpointOverride URI kmsEndpointOverride) {
    try {
      var client =
          new AwsKmsV2Client()
              .withHttpClient(httpClient)
              .withCredentialsProvider(credentialsProvider);
      if (!kmsEndpointOverride.toString().isEmpty()) {
        client.withKmsEndpointOverride(kmsEndpointOverride);
      }
      return client.getAead(keyEncryptionKeyUri);
    } catch (GeneralSecurityException e) {
      throw new AssertionError("Failed to build AwsKmsV2Client", e);
    }
  }

  /** Provides a singleton of the {@code HttpClient} class. */
  @Provides
  @Singleton
  @CoordinatorBHttpClient
  public HttpClient provideHttpClient(
      @CoordinatorBCredentialsProvider AwsSessionCredentialsProvider credentialsProvider) {
    return HttpClients.custom()
        .addInterceptorFirst(
            AwsRequestSigner.createRequestSignerInterceptor(
                args.getCoordinatorBRegion(), credentialsProvider))
        .setServiceUnavailableRetryStrategy(DefaultHttpClientRetryStrategy.getInstance())
        .build();
  }

  /** Provider for an instance of the {@code SqsClient} class. */
  @Provides
  @Singleton
  SqsClient provideSqsClient(
      AwsCredentialsProvider credentials,
      SdkHttpClient httpClient,
      RetryPolicy retryPolicy,
      @KeyGenerationSqsEndpointOverrideBinding URI endpointOverride,
      @ApplicationRegionBinding String region) {
    SqsClientBuilder clientBuilder =
        SqsClient.builder()
            .credentialsProvider(credentials)
            .httpClient(httpClient)
            .overrideConfiguration(
                ClientOverrideConfiguration.builder().retryPolicy(retryPolicy).build())
            .region(Region.of(region));

    if (!endpointOverride.toString().isEmpty()) {
      clientBuilder.endpointOverride(endpointOverride);
    }

    return clientBuilder.build();
  }

  @Provides
  @Singleton
  StsClient provideStsClient(
      AwsCredentialsProvider credentials,
      SdkHttpClient httpClient,
      RetryPolicy retryPolicy,
      @ApplicationRegionBinding String region) {
    return StsClient.builder()
        .credentialsProvider(credentials)
        .httpClient(httpClient)
        .overrideConfiguration(
            ClientOverrideConfiguration.builder().retryPolicy(retryPolicy).build())
        .region(Region.of(region))
        .applyMutation(
            (builder) -> args.getStsEndpointOverride().ifPresent(builder::endpointOverride))
        .build();
  }

  @Provides
  @KeyGenerationSqsUrl
  String provideKeyGenerationSqsUrl(ParameterClient paramClient) throws ParameterClientException {
    return paramClient
        .getParameter(KEY_GENERATION_QUEUE_URL)
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "No Key Generation SQS Url in parameter client.",
                    ErrorReason.MISSING_REQUIRED_PARAMETER));
  }

  @Provides
  @KeyStorageServiceBaseUrl
  String provideKeyStorageServiceBaseUrl(ParameterClient paramClient)
      throws ParameterClientException {
    return paramClient
        .getParameter(KEY_STORAGE_SERVICE_BASE_URL)
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "No Key Storage Service Base Url in parameter client.",
                    ErrorReason.MISSING_REQUIRED_PARAMETER));
  }

  @Provides
  @KeyEncryptionKeyUri
  String provideKeyEncryptionKeyUri(ParameterClient paramClient) throws ParameterClientException {
    return paramClient
        .getParameter(KMS_KEY_URI)
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "No KMS Key Uri in parameter client.", ErrorReason.MISSING_REQUIRED_PARAMETER));
  }

  @Provides
  public KmsClient provideKmsClient(
      AwsCredentialsProvider credentialsProvider,
      SdkHttpClient httpClient,
      @KmsEndpointOverride URI kmsEndpointOverride,
      @ApplicationRegionBinding String region) {
    var client =
        KmsClient.builder()
            .region(Region.of(region))
            .credentialsProvider(credentialsProvider)
            .httpClient(httpClient);
    if (!kmsEndpointOverride.toString().isEmpty()) {
      client.endpointOverride(kmsEndpointOverride);
    }
    return client.build();
  }

  @Provides
  @Singleton
  @EncryptionKeySignatureKey
  Optional<PublicKeySign> provideSignatureKey(ParameterClient paramClient, KmsClient kmsClient)
      throws ParameterClientException {
    Optional<String> signatureKeyId = paramClient.getParameter(ENCRYPTION_KEY_SIGNATURE_KEY_ID);
    if (signatureKeyId.isPresent()) {
      String signatureAlgorithm =
          paramClient
              .getParameter(ENCRYPTION_KEY_SIGNATURE_ALGORITHM)
              .orElseThrow(
                  () ->
                      new ParameterClientException(
                          "No Signature Key Algorithm in parameter client, when Signature Key Uri"
                              + " was given",
                          ErrorReason.MISSING_REQUIRED_PARAMETER));
      return Optional.of(
          new AwsKmsPublicKeySign(kmsClient, signatureKeyId.get(), signatureAlgorithm));
    } else {
      return Optional.empty();
    }
  }

  @Provides
  @DynamoKeyDbTableName
  String provideDynamoKeyDbTableName(ParameterClient paramClient) throws ParameterClientException {
    return paramClient
        .getParameter(KEY_DB_NAME)
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "No DynamoKeyDb Table Name in parameter client.",
                    ErrorReason.MISSING_REQUIRED_PARAMETER));
  }

  @Provides
  @DynamoKeyDbRegion
  String provideDynamoKeyDbRegion(ParameterClient paramClient) throws ParameterClientException {
    return paramClient
        .getParameter(KEY_DB_REGION)
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "No DynamoKeyDb Region in parameter client.",
                    ErrorReason.MISSING_REQUIRED_PARAMETER));
  }

  @Provides
  @Singleton
  @CoordinatorBCredentialsProvider
  AwsSessionCredentialsProvider provideAwsSessionCredentialsProvider(
      StsClient stsClient, AwsCredentialsProvider credentials, ParameterClient paramClient) {
    try {
      return new StsAwsSessionCredentialsProvider(
          stsClient,
          paramClient.getParameter(COORDINATOR_B_ASSUME_ROLE_ARN).get(),
          "keyGenerationService");
    } catch (ParameterClientException | NoSuchElementException e) {
      return new SelfAwsSessionCredentialsProvider(credentials, stsClient);
    }
  }

  @Provides
  @Singleton
  CloudAeadSelector provideCloudAeadSelector(
      @CoordinatorBCredentialsProvider AwsSessionCredentialsProvider credentialsProvider,
      SdkHttpClient httpClient) {
    switch (args.getDecryptionClient()) {
      case ENCLAVE:
        return AwsTinkUtil.getEnclaveAeadSelector(credentialsProvider);
      case NON_ENCLAVE:
        var endpointOverride =
            Optional.of(args.getKmsEndpointOverride())
                .filter(endpoint -> !endpoint.toString().isEmpty());
        return AwsTinkUtil.getKmsAeadSelector(credentialsProvider, httpClient, endpointOverride);
      default:
        throw new IllegalArgumentException(
            String.format("Invalid decryption client: %s", args.getDecryptionClient()));
    }
  }

  @Provides
  @KeyIdTypeName
  Optional<String> provideKeyIdTypeProvider(ParameterClient paramClient)
      throws ParameterClientException {
    return paramClient
        .getParameter(KEY_ID_TYPE)
        .filter(type -> !type.isEmpty())
        .or(args::getKeyIdType);
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
}
