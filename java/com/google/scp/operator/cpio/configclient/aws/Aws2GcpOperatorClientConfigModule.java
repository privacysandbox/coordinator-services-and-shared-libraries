/*
 * Copyright 2024 Google LLC
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

package com.google.scp.operator.cpio.configclient.aws;

import static com.google.scp.operator.cpio.configclient.common.ConfigClientUtil.COORDINATOR_HTTPCLIENT_MAX_ATTEMPTS;
import static com.google.scp.operator.cpio.configclient.common.ConfigClientUtil.COORDINATOR_HTTPCLIENT_RETRY_INITIAL_INTERVAL;
import static com.google.scp.operator.cpio.configclient.common.ConfigClientUtil.COORDINATOR_HTTPCLIENT_RETRY_MULTIPLIER;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorACredentials;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorAHttpClient;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorARegionBinding;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorARegionBindingOverride;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBCredentials;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBHttpClient;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBRegionBinding;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBRegionBindingOverride;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorACredentialsProvider;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorARoleArn;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorAWifCredentialConfig;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorAWifSaEmail;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorBCredentialsProvider;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorBRoleArn;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorBWifCredentialConfig;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorBWifSaEmail;
import com.google.scp.operator.cpio.configclient.common.OperatorClientConfig;
import com.google.scp.shared.api.util.HttpClientWrapper;
import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import com.google.scp.shared.aws.credsprovider.StsAwsSessionCredentialsProvider;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule;
import com.google.scp.shared.clients.configclient.gcp.CredentialsHelper;
import com.google.scp.shared.clients.configclient.model.ErrorReason;
import com.google.scp.shared.clients.configclient.model.WorkerParameter;
import com.google.scp.shared.gcp.util.GcpHttpInterceptorUtil;
import java.io.IOException;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.sts.StsClient;

/**
 * This Guice module provides necessary configurations for AWS services clients running as
 * operators/adtech that use GCP Coordinators.
 */
public final class Aws2GcpOperatorClientConfigModule extends AbstractModule {

  private static final String SESSION_NAME = "enclave_worker";

  /** Configures injected dependencies for this module. */
  @Override
  protected void configure() {
    install(new AwsClientConfigModule());
  }

  /**
   * Provider for a singleton of the {@code GoogleCredentials} class which represents federated
   * credentials for CoordinatorA.
   */
  @Provides
  @CoordinatorACredentials
  @Singleton
  GoogleCredentials provideCoordinatorACredentials(
      @CoordinatorAWifCredentialConfig String coordinatorAWifCredentialConfig,
      @CoordinatorAWifSaEmail String coordinatorAWifSaEmail)
      throws IOException {
    return CredentialsHelper.getAwsWifCredentials(
        coordinatorAWifCredentialConfig, coordinatorAWifSaEmail);
  }

  /**
   * Provider for a singleton of the {@code GoogleCredentials} class which represents federated
   * credentials for CoordinatorB.
   */
  @Provides
  @CoordinatorBCredentials
  @Singleton
  GoogleCredentials provideCoordinatorBCredentials(
      @CoordinatorBWifCredentialConfig String coordinatorBWifCredentialConfig,
      @CoordinatorBWifSaEmail String coordinatorBWifSaEmail)
      throws IOException {
    return CredentialsHelper.getAwsWifCredentials(
        coordinatorBWifCredentialConfig, coordinatorBWifSaEmail);
  }

  /** Provider for an {@code HttpClientWrapper} to access coordinator A. */
  @Provides
  @Singleton
  @CoordinatorAHttpClient
  public HttpClientWrapper provideCoordinatorAHttpClient(
      OperatorClientConfig config,
      @CoordinatorACredentials GoogleCredentials coordinatorACredentials) {
    return getHttpClientWrapper(
        coordinatorACredentials,
        config
            .coordinatorAEncryptionKeyServiceCloudfunctionUrl()
            .orElse(config.coordinatorAEncryptionKeyServiceBaseUrl()));
  }

  /** Provider for an {@code HttpClientWrapper} to access coordinator B. */
  @Provides
  @Singleton
  @CoordinatorBHttpClient
  public HttpClientWrapper provideCoordinatorBHttpClient(
      OperatorClientConfig config,
      @CoordinatorBCredentials GoogleCredentials coordinatorBCredentials) {
    return getHttpClientWrapper(
        coordinatorBCredentials,
        config
            .coordinatorBEncryptionKeyServiceCloudfunctionUrl()
            .orElse(config.coordinatorBEncryptionKeyServiceBaseUrl()));
  }

  /** Provider for a {@code AwsSessionCredentialsProvider} class to access coordinator A. */
  @Provides
  @Singleton
  @CoordinatorACredentialsProvider
  AwsSessionCredentialsProvider provideCoordinatorACredentialsProvider(
      StsClient client, @CoordinatorARoleArn String roleArn) {
    return new StsAwsSessionCredentialsProvider(client, roleArn, SESSION_NAME);
  }

  /** Provider for a {@code AwsSessionCredentialsProvider} class to access coordinator B. */
  @Provides
  @Singleton
  @CoordinatorBCredentialsProvider
  AwsSessionCredentialsProvider provideCoordinatorBCredentialsProvider(
      StsClient client, @CoordinatorBRoleArn String roleArn) {
    return new StsAwsSessionCredentialsProvider(client, roleArn, SESSION_NAME);
  }

  /** Provider for a String containing the ARN of the assumed role. */
  @Provides
  @CoordinatorARoleArn
  String provideCoordinatorARoleArn(ParameterClient paramClient) throws ParameterClientException {
    return paramClient
        .getParameter(WorkerParameter.COORDINATOR_A_ROLE.name())
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "Could not get coordinator A assume role ARN from the parameter client.",
                    ErrorReason.MISSING_REQUIRED_PARAMETER));
  }

  /** Provider for a String containing the ARN of the assumed role. */
  @Provides
  @CoordinatorBRoleArn
  String provideCoordinatorBRoleArn(ParameterClient paramClient) throws ParameterClientException {
    return paramClient
        .getParameter(WorkerParameter.COORDINATOR_B_ROLE.name())
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "Could not get coordinator B assume role ARN from the parameter client.",
                    ErrorReason.MISSING_REQUIRED_PARAMETER));
  }

  /** Provider for a {@code Region} singleton containing coordinator A's region. */
  @Provides
  @Singleton
  @CoordinatorARegionBinding
  public Region provideCoordinatorARegion(@CoordinatorARegionBindingOverride String region) {
    return Region.of(region);
  }

  /** Provider for a {@code Region} singleton containing coordinator B's region. */
  @Provides
  @Singleton
  @CoordinatorBRegionBinding
  public Region provideCoordinatorBRegion(@CoordinatorBRegionBindingOverride String region) {
    return Region.of(region);
  }

  /**
   * Provider for a String containing coordinator A's Workload Identity Federation configuration.
   */
  @Provides
  @CoordinatorAWifCredentialConfig
  String provideCoordinatorAWifConfig(ParameterClient paramClient) throws ParameterClientException {
    return paramClient
        .getParameter(WorkerParameter.COORDINATOR_A_WIF_CONFIG.name())
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "Could not get coordinator A workload identity federation configuration.",
                    ErrorReason.MISSING_REQUIRED_PARAMETER));
  }

  /**
   * Provider for a String containing coordinator B's Workload Identity Federation configuration.
   */
  @Provides
  @CoordinatorBWifCredentialConfig
  String provideCoordinatorBWifConfig(ParameterClient paramClient) throws ParameterClientException {
    return paramClient
        .getParameter(WorkerParameter.COORDINATOR_B_WIF_CONFIG.name())
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "Could not get coordinator B workload identity federation configuration.",
                    ErrorReason.MISSING_REQUIRED_PARAMETER));
  }

  /** Provider for a coordinator A's Workload Identity Federation Service Account email. */
  @Provides
  @CoordinatorAWifSaEmail
  String provideCoordinatorAWifSaEmail(ParameterClient paramClient)
      throws ParameterClientException {
    return paramClient
        .getParameter(WorkerParameter.COORDINATOR_A_WIF_SA_EMAIL.name())
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "Could not get coordinator A workload identity federation service account"
                        + " email.",
                    ErrorReason.MISSING_REQUIRED_PARAMETER));
  }

  /** Provider for a coordinator B's Workload Identity Federation Service Account email. */
  @Provides
  @CoordinatorBWifSaEmail
  String provideCoordinatorBWifSaEmail(ParameterClient paramClient)
      throws ParameterClientException {
    return paramClient
        .getParameter(WorkerParameter.COORDINATOR_B_WIF_SA_EMAIL.name())
        .orElseThrow(
            () ->
                new ParameterClientException(
                    "Could not get coordinator B workload identity federation service account"
                        + " email.",
                    ErrorReason.MISSING_REQUIRED_PARAMETER));
  }

  private static HttpClientWrapper getHttpClientWrapper(GoogleCredentials credentials, String url) {
    return HttpClientWrapper.builder()
        .setInterceptor(GcpHttpInterceptorUtil.createHttpInterceptor(credentials, url))
        .setExponentialBackoff(
            COORDINATOR_HTTPCLIENT_RETRY_INITIAL_INTERVAL,
            COORDINATOR_HTTPCLIENT_RETRY_MULTIPLIER,
            COORDINATOR_HTTPCLIENT_MAX_ATTEMPTS)
        .build();
  }
}
