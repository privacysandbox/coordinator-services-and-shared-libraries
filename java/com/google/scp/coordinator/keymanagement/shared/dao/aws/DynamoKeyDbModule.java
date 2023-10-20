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

package com.google.scp.coordinator.keymanagement.shared.dao.aws;

import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.keymanagement.shared.dao.aws.Annotations.DynamoKeyDbRegion;
import com.google.scp.coordinator.keymanagement.shared.dao.common.Annotations.KeyDbClient;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import java.net.URI;
import java.util.Optional;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.http.SdkHttpClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClientBuilder;

/** Module for dynamo key db. */
public final class DynamoKeyDbModule extends AbstractModule {

  private final Optional<String> endpointOverride;

  public DynamoKeyDbModule(Optional<String> endpointOverride) {
    this.endpointOverride = endpointOverride;
  }

  @Provides
  @Singleton
  @KeyDbClient
  public DynamoDbEnhancedClient provideDynamoDbEnhanced(
      @DynamoKeyDbRegion String region,
      AwsCredentialsProvider credentials,
      SdkHttpClient httpClient) {
    DynamoDbClientBuilder builder =
        DynamoDbClient.builder()
            .credentialsProvider(credentials)
            .httpClient(httpClient)
            .region(Region.of(region));
    // Add endpoint if present
    this.endpointOverride.map(URI::create).ifPresent(builder::endpointOverride);
    return DynamoDbEnhancedClient.builder().dynamoDbClient(builder.build()).build();
  }

  @Override
  protected void configure() {
    bind(KeyDb.class).to(DynamoKeyDb.class);
  }
}
