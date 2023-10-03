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

package com.google.scp.operator.cpio.blobstorageclient.aws;

import com.google.inject.BindingAnnotation;
import com.google.inject.Key;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.inject.multibindings.OptionalBinder;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClientModule;
import com.google.scp.shared.clients.configclient.Annotations.ApplicationRegionBinding;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.net.URI;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.core.client.config.ClientOverrideConfiguration;
import software.amazon.awssdk.core.retry.RetryPolicy;
import software.amazon.awssdk.http.apache.ApacheHttpClient;
import software.amazon.awssdk.http.nio.netty.NettyNioAsyncHttpClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.s3.S3AsyncClient;
import software.amazon.awssdk.services.s3.S3AsyncClientBuilder;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.s3.S3ClientBuilder;
import software.amazon.awssdk.services.s3.S3Configuration;

/** Guice Module for the S3 implementation of {@link BlobStorageClient} */
public final class S3BlobStorageClientModule extends BlobStorageClientModule {

  /** Returns {@code S3BlobStorageClient} as the implementation for {@code BlobStorageClient} */
  @Override
  public Class<? extends BlobStorageClient> getBlobStorageClientImplementation() {
    return S3BlobStorageClient.class;
  }

  /** Provider for an {@code S3Client} singleton */
  @Provides
  @Singleton
  S3Client provideS3Client(
      AwsCredentialsProvider awsCredentialsProvider,
      RetryPolicy retryPolicy,
      @S3EndpointOverrideBinding URI endpointOverride,
      @ApplicationRegionBinding String region) {
    // TODO path style access will be deprecated so this is not a long term solution.
    S3ClientBuilder builder =
        S3Client.builder()
            .httpClientBuilder(ApacheHttpClient.builder().maxConnections(100))
            .credentialsProvider(awsCredentialsProvider)
            .overrideConfiguration(
                ClientOverrideConfiguration.builder().retryPolicy(retryPolicy).build())
            .serviceConfiguration(S3Configuration.builder().pathStyleAccessEnabled(true).build())
            .region(Region.of(region));
    if (!endpointOverride.toString().isEmpty()) {
      builder.endpointOverride(endpointOverride);
    }
    return builder.build();
  }

  /** Provider for an {@code S3AsyncClient} singleton */
  @Provides
  @Singleton
  S3AsyncClient providesS3AsyncClient(
      AwsCredentialsProvider awsCredentialsProvider,
      RetryPolicy retryPolicy,
      @S3EndpointOverrideBinding URI endpointOverride,
      @ApplicationRegionBinding String region) {
    S3AsyncClientBuilder builder =
        S3AsyncClient.builder()
            .httpClient(NettyNioAsyncHttpClient.builder().maxConcurrency(100).build())
            .credentialsProvider(awsCredentialsProvider)
            .overrideConfiguration(
                ClientOverrideConfiguration.builder().retryPolicy(retryPolicy).build())
            .serviceConfiguration(S3Configuration.builder().pathStyleAccessEnabled(true).build())
            .region(Region.of(region));
    if (!endpointOverride.toString().isEmpty()) {
      builder.endpointOverride(endpointOverride);
    }
    return builder.build();
  }

  @Override
  public void configureModule() {
    // Default binding for S3UsePartialRequests
    OptionalBinder.newOptionalBinder(binder(), Key.get(Boolean.class, S3UsePartialRequests.class))
        .setDefault()
        .toInstance(false);
    // Default binding for PartialRequestBufferSize
    OptionalBinder.newOptionalBinder(
            binder(), Key.get(Integer.class, PartialRequestBufferSize.class))
        .setDefault()
        .toInstance(1800000);
  }

  /** Annotation for a binding to override the S3 endpoint. */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface S3EndpointOverrideBinding {}

  /** Annotations for a binding that indicates whether to use http partial request. */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface S3UsePartialRequests {}

  /** Annotations for a binding that specifies the http partial request size. */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface PartialRequestBufferSize {}
}
