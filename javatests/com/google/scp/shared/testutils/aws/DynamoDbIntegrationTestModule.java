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

package com.google.scp.shared.testutils.aws;

import com.google.inject.AbstractModule;
import com.google.inject.BindingAnnotation;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.net.URI;
import java.time.Duration;
import org.testcontainers.containers.GenericContainer;
import org.testcontainers.containers.wait.strategy.Wait;
import software.amazon.awssdk.auth.credentials.AwsBasicCredentials;
import software.amazon.awssdk.auth.credentials.StaticCredentialsProvider;
import software.amazon.awssdk.http.SdkHttpClient;
import software.amazon.awssdk.http.apache.ApacheHttpClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;

/**
 * Module which provides a {@link DynamoDbClient} that uses the offical amazon dynamodb-local
 * container.
 *
 * <p>Tests which require multiple AWS clients (e.g. AWS, KMS) should instead install {@link
 * AwsIntegrationTestModule} which instead uses localstack.
 *
 * <p>The dynamodb container started by this module only takes ~5 seconds to start compared to the
 * ~35 seconds the localstack-based {@link AwsIntegrationTestModule} takes.
 */
// Ignore warnings around using GenericContainer as a raw type.
@SuppressWarnings("rawtypes")
public final class DynamoDbIntegrationTestModule extends AbstractModule {
  private static final Duration HTTP_CLIENT_CONNECTION_TIMEOUT_DURATION = Duration.ofSeconds(10);
  private static final Duration HTTP_CLIENT_SOCKET_TIMEOUT_DURATION = Duration.ofSeconds(10);
  private static final Region DYNAMO_TABLE_REGION = Region.US_EAST_1;
  private static final String CONTAINER_IMAGE = "amazon/dynamodb-local:1.16.0";

  /**
   * HTTP Client with increased timeouts for better resiliency in tests and TCP keepalives enabled
   * for increased performance.
   */
  private static final SdkHttpClient defaultHttpClient =
      ApacheHttpClient.builder()
          .connectionTimeout(HTTP_CLIENT_CONNECTION_TIMEOUT_DURATION)
          .socketTimeout(HTTP_CLIENT_SOCKET_TIMEOUT_DURATION)
          .tcpKeepAlive(true)
          .build();

  @Provides
  @Singleton
  @DynamoDbContainer
  // Ignore autoclosable warning, the container will be closed by ryuk when the JVM shuts down.
  @SuppressWarnings("resource")
  public GenericContainer provideDynamoDbContainer() {
    var container = new GenericContainer(CONTAINER_IMAGE).withExposedPorts(8000);
    container.start();
    container.waitingFor(Wait.forHealthcheck());
    return container;
  }

  @Provides
  public DynamoDbClient getDynamoDbClient(@DynamoDbContainer GenericContainer container) {
    return DynamoDbClient.builder()
        .endpointOverride(URI.create("http://localhost:" + container.getFirstMappedPort()))
        .region(DYNAMO_TABLE_REGION)
        // dynamodb-local accepts any credentials but credentials must be present and non-empty.
        .credentialsProvider(
            StaticCredentialsProvider.create(AwsBasicCredentials.create("fakeid", "fakesecret")))
        .httpClient(defaultHttpClient)
        .build();
  }

  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  private @interface DynamoDbContainer {}
}
