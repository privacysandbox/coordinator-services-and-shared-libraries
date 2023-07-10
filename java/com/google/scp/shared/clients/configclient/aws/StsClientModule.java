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

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.inject.AbstractModule;
import com.google.inject.BindingAnnotation;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;
import java.net.URI;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.http.apache.ApacheHttpClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.sts.StsClient;

/** Simple module for providing an {@link StsClient}. */
public final class StsClientModule extends AbstractModule {
  private static final Region STS_REGION = Region.of("us-east-1");

  /** Provider for a singleton of the {@code StsClient} class. */
  @Provides
  @Singleton
  StsClient provideStsClient(
      AwsCredentialsProvider awsCredentialsProvider, @StsEndpointOverrideBinding URI stsEndpoint) {
    var builder =
        StsClient.builder()
            .region(STS_REGION)
            .httpClient(ApacheHttpClient.builder().build())
            .credentialsProvider(awsCredentialsProvider);

    if (!stsEndpoint.toString().isEmpty()) {
      builder.endpointOverride(stsEndpoint);
    }

    return builder.build();
  }

  /** Annotation for binding an override of the STS endpoint. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface StsEndpointOverrideBinding {}
}
