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

package com.google.scp.operator.cpio.configclient.aws;

import static com.google.scp.operator.cpio.configclient.common.ConfigClientUtil.COORDINATOR_HTTPCLIENT_MAX_ATTEMPTS;
import static com.google.scp.operator.cpio.configclient.common.ConfigClientUtil.COORDINATOR_HTTPCLIENT_RETRY_INITIAL_INTERVAL;
import static com.google.scp.operator.cpio.configclient.common.ConfigClientUtil.COORDINATOR_HTTPCLIENT_RETRY_MULTIPLIER;

import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorAHttpClient;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorARegionBinding;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorARegionBindingOverride;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBHttpClient;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBRegionBinding;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBRegionBindingOverride;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorACredentialsProvider;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorARoleArn;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorBCredentialsProvider;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorBRoleArn;
import com.google.scp.shared.api.util.HttpClientWrapper;
import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import com.google.scp.shared.aws.credsprovider.StsAwsSessionCredentialsProvider;
import com.google.scp.shared.aws.util.AwsRequestSigner;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsCredentialAccessKey;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsCredentialSecretKey;
import com.google.scp.shared.clients.configclient.model.ErrorReason;
import com.google.scp.shared.clients.configclient.model.WorkerParameter;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.sts.StsClient;

/**
 * This Guice module provides necessary configurations for AWS services clients running as
 * operators/adtech.
 */
public final class AwsOperatorClientConfigModule extends AbstractModule {

  private static final String SESSION_NAME = "enclave_worker";

  /** Configures injected dependencies for this module. */
  @Override
  protected void configure() {
    install(new AwsClientConfigModule());
  }

  /** Provider for an {@code HttpClientWrapper} to access coordinator A. */
  @Provides
  @Singleton
  @CoordinatorAHttpClient
  public HttpClientWrapper provideCoordinatorAHttpClient(
      @AwsCredentialAccessKey String accessKey,
      @AwsCredentialSecretKey String secretKey,
      @CoordinatorARegionBinding Region coordinatorRegion,
      @CoordinatorACredentialsProvider AwsSessionCredentialsProvider credentialsProvider) {
    if (!accessKey.isEmpty() && !secretKey.isEmpty()) {
      // Doesn't use STS, so requests can't be signed (no session token)
      return HttpClientWrapper.createDefault();
    } else {
      return getHttpClientWrapper(coordinatorRegion, credentialsProvider);
    }
  }

  /** Provider for an {@code HttpClientWrapper} to access coordinator B. */
  @Provides
  @Singleton
  @CoordinatorBHttpClient
  public HttpClientWrapper provideCoordinatorBHttpClient(
      @AwsCredentialAccessKey String accessKey,
      @AwsCredentialSecretKey String secretKey,
      @CoordinatorBRegionBinding Region coordinatorRegion,
      @CoordinatorBCredentialsProvider AwsSessionCredentialsProvider credentialsProvider) {
    if (!accessKey.isEmpty() && !secretKey.isEmpty()) {
      // Doesn't use STS, so requests can't be signed (no session token)
      return HttpClientWrapper.createDefault();
    } else {
      return getHttpClientWrapper(coordinatorRegion, credentialsProvider);
    }
  }

  /** Provided for backwards compatibility with single key clients. */
  @Provides
  @Singleton
  public HttpClientWrapper provideCoordinatorHttpClient(
      @CoordinatorAHttpClient HttpClientWrapper httpClientWrapper) {
    return httpClientWrapper;
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

  /** Provided for backwards compatibility with single key clients. */
  @Provides
  @Singleton
  AwsSessionCredentialsProvider provideCredentialsProvider(
      @CoordinatorACredentialsProvider AwsSessionCredentialsProvider credentialsProvider) {
    return credentialsProvider;
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

  private static HttpClientWrapper getHttpClientWrapper(
      Region coordinatorRegion, AwsSessionCredentialsProvider credentialsProvider) {
    return HttpClientWrapper.builder()
        .setInterceptor(
            AwsRequestSigner.createRequestSignerInterceptor(coordinatorRegion, credentialsProvider))
        .setExponentialBackoff(
            COORDINATOR_HTTPCLIENT_RETRY_INITIAL_INTERVAL,
            COORDINATOR_HTTPCLIENT_RETRY_MULTIPLIER,
            COORDINATOR_HTTPCLIENT_MAX_ATTEMPTS)
        .build();
  }
}
