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

package com.google.scp.operator.cpio.metricclient.aws;

import com.google.inject.BindingAnnotation;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.metricclient.MetricClient;
import com.google.scp.operator.cpio.metricclient.MetricModule;
import com.google.scp.shared.clients.configclient.Annotations.ApplicationRegionBinding;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.net.URI;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.core.client.config.ClientOverrideConfiguration;
import software.amazon.awssdk.core.retry.RetryPolicy;
import software.amazon.awssdk.http.SdkHttpClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.cloudwatch.CloudWatchClient;
import software.amazon.awssdk.services.cloudwatch.CloudWatchClientBuilder;

/** Guice module for binding the AWS lifecycle client functionality */
@Singleton
public final class AwsMetricModule extends MetricModule {

  @Override
  public Class<? extends MetricClient> getMetricClientImpl() {
    return AwsMetricClient.class;
  }

  /** Provider for a singleton instance of the {@code CloudWatchClient} class. */
  @Provides
  @Singleton
  CloudWatchClient provideCloudWatchClient(
      AwsCredentialsProvider credentials,
      SdkHttpClient httpClient,
      RetryPolicy retryPolicy,
      @CloudwatchEndpointOverrideBinding URI endpointOverride,
      @ApplicationRegionBinding String region) {
    CloudWatchClientBuilder clientBuilder =
        CloudWatchClient.builder()
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

  /** Annotation for binding a CloudWatch endpoint override. */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface CloudwatchEndpointOverrideBinding {}
}
