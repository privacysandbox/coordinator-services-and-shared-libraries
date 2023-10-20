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

package com.google.scp.shared.clients.configclient.aws;

import com.google.inject.AbstractModule;
import com.google.inject.BindingAnnotation;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.shared.clients.configclient.Annotations.ApplicationRegionBinding;
import com.google.scp.shared.clients.configclient.Annotations.ApplicationRegionBindingOverride;
import io.github.resilience4j.core.IntervalFunction;
import io.github.resilience4j.retry.Retry;
import io.github.resilience4j.retry.RetryConfig;
import io.github.resilience4j.retry.RetryRegistry;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.time.Duration;
import software.amazon.awssdk.auth.credentials.AwsBasicCredentials;
import software.amazon.awssdk.auth.credentials.AwsCredentials;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.DefaultCredentialsProvider;
import software.amazon.awssdk.auth.credentials.StaticCredentialsProvider;
import software.amazon.awssdk.core.SdkSystemSetting;
import software.amazon.awssdk.core.retry.RetryPolicy;
import software.amazon.awssdk.core.retry.backoff.EqualJitterBackoffStrategy;
import software.amazon.awssdk.regions.internal.util.EC2MetadataUtils;

/** This Guice module provides necessary configurations for AWS services clients. */
public class AwsClientConfigModule extends AbstractModule {

  // Number of times client will retry before failing.
  public static final int CLIENT_NUM_RETRIES = 5;
  // Maximum wait time for exponential backoff retry policy.
  public static final Duration CLIENT_MAX_BACKOFF_TIME = Duration.ofSeconds(30);
  // The base delay for exponential backoff retry policy.
  public static final Duration CLIENT_BASE_DELAY = Duration.ofSeconds(2);

  /**
   * Provides a {@link RetryPolicy} object for AWS service clients.
   *
   * <p>The retry policy uses an exponential backoff strategy, with the backoff delay computed with
   * the following formula:
   *
   * <ul>
   *   <li>X = min(2^(num previous retries)*CLIENT_BASE_DELAY, CLIENT_MAX_BACKOFF_TIME)
   *   <li>delay = X/2 + rand(0, X/2)
   */
  @Provides
  RetryPolicy provideRetryPolicy() {
    return RetryPolicy.builder()
        .numRetries(CLIENT_NUM_RETRIES)
        .backoffStrategy(
            EqualJitterBackoffStrategy.builder()
                .maxBackoffTime(CLIENT_MAX_BACKOFF_TIME)
                .baseDelay(CLIENT_BASE_DELAY)
                .build())
        .build();
  }

  /**
   * Provides an {@link AwsCredentialsProvider} object.
   *
   * <p>Chooses what type of credential provider based on which annotations were configured. The
   * returned credentials provider can be one of the following:
   *
   * <ul>
   *   <li>{@link StaticCredentialsProvider}: for E2E testing, if we specify access key and secret
   *       key in the input arguments.
   *   <li>{@link InstanceProfileCredentialsWithRetryProvider}: for enclave env, if we specify aws
   *       credentials provider endpoint override in the input arguments.
   *   <li>{@link DefaultCredentialsProvider}: for local run outside the enclave, it looks of
   *       various places to find the credentials. Find out more about this here:
   *       https://docs.aws.amazon.com/sdk-for-java/latest/developer-guide/credentials.html
   * </ul>
   *
   * <p>AWS SDK credentials providers should all be thread-safe, according to this link:
   * https://stackoverflow.com/questions/43944700/is-defaultawscredentialsproviderchain-v1-11-124-thread-safe
   */
  @Provides
  @Singleton
  AwsCredentialsProvider provideCredentialsProvider(
      @AwsCredentialAccessKey String accessKey,
      @AwsCredentialSecretKey String secretKey,
      @AwsEc2MetadataEndpointOverride String metadataEndpointOverride) {
    if (!accessKey.isEmpty() && !secretKey.isEmpty()) {
      return StaticCredentialsProvider.create(AwsBasicCredentials.create(accessKey, secretKey));
    }

    // Within enclave, vsock proxy forwards from "metadataEndpointOverride" to the EC2 metadata
    // endpoint.
    if (!metadataEndpointOverride.isEmpty()) {
      System.setProperty(
          SdkSystemSetting.AWS_EC2_METADATA_SERVICE_ENDPOINT.property(), metadataEndpointOverride);

      // Build a retry configuration that retries up to 4 times on exceptions
      // First retry waits 100ms, then doubles each retry after
      RetryRegistry registry =
          RetryRegistry.of(
              RetryConfig.<AwsCredentials>custom()
                  .maxAttempts(4)
                  .intervalFunction(
                      IntervalFunction.ofExponentialBackoff(Duration.ofMillis(100), 2))
                  .retryExceptions(RuntimeException.class)
                  .build());
      Retry retry = registry.retry("credentialsRetryConfig");

      return InstanceProfileCredentialsWithRetryProvider.builder()
          .endpoint(metadataEndpointOverride)
          .retryConfig(retry)
          .build();
    }

    return DefaultCredentialsProvider.create();
  }

  /** Provider for a String containing the region of this client. */
  @Provides
  @Singleton
  @ApplicationRegionBinding
  String provideApplicationRegion(@ApplicationRegionBindingOverride String regionOverride) {
    if (regionOverride.isEmpty()) {
      return EC2MetadataUtils.getEC2InstanceRegion();
    } else {
      return regionOverride;
    }
  }

  /** Annotation for the EC2 Metadata endpoint override. */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface AwsEc2MetadataEndpointOverride {}

  /** Annotation for the AWS credential access key. */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface AwsCredentialAccessKey {}

  /** Annotation for the AWS credential secret key. */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface AwsCredentialSecretKey {}
}
