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

import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.crypto.tink.aws.AwsTinkUtil;
import java.time.Duration;
import java.util.Optional;
import org.testcontainers.containers.localstack.LocalStackContainer;
import software.amazon.awssdk.auth.credentials.AwsBasicCredentials;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.StaticCredentialsProvider;
import software.amazon.awssdk.core.client.config.ClientOverrideConfiguration;
import software.amazon.awssdk.core.retry.RetryPolicy;
import software.amazon.awssdk.core.retry.backoff.EqualJitterBackoffStrategy;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.http.SdkHttpClient;
import software.amazon.awssdk.http.apache.ApacheHttpClient;
import software.amazon.awssdk.http.async.SdkAsyncHttpClient;
import software.amazon.awssdk.http.nio.netty.NettyNioAsyncHttpClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.apigateway.ApiGatewayClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.streams.DynamoDbStreamsClient;
import software.amazon.awssdk.services.kms.KmsClient;
import software.amazon.awssdk.services.lambda.LambdaClient;
import software.amazon.awssdk.services.s3.S3AsyncClient;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.sqs.SqsClient;
import software.amazon.awssdk.services.sts.StsClient;

/**
 * Helper class for providing AWS clients configured to talk to LocalStack with test-friendly
 * configurations for retries and timeouts.
 *
 * <p>Methods should be free of side effects (e.g. creating tables or buckets) and only return the
 * configured client.
 */
public final class LocalStackAwsClientUtil {
  private static final Region REGION = Region.US_EAST_1;
  private static final Duration HTTP_CLIENT_CONNECTION_TIMEOUT_DURATION = Duration.ofSeconds(10);
  private static final Duration HTTP_CLIENT_SOCKET_TIMEOUT_DURATION = Duration.ofSeconds(10);
  private static final int RETRY_POLICY_MAX_RETRIES = 5;
  // Maximum wait time for exponential backoff retry policy.
  private static final Duration CLIENT_MAX_BACKOFF_TIME = Duration.ofSeconds(10);
  // The base delay for exponential backoff retry policy.
  private static final Duration CLIENT_BASE_DELAY = Duration.ofSeconds(2);

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

  private static final SdkAsyncHttpClient defaultAsyncHttpClient =
      NettyNioAsyncHttpClient.builder()
          .connectionTimeout(HTTP_CLIENT_CONNECTION_TIMEOUT_DURATION)
          .tcpKeepAlive(true)
          .build();

  // Invoking a Lambda can take more than 10 seconds, increase timeouts.
  private static final SdkHttpClient lambdaHttpClient =
      ApacheHttpClient.builder()
          // Note: 30 seconds appeared to cause premature timeouts.
          .connectionTimeout(Duration.ofSeconds(120))
          .socketTimeout(Duration.ofSeconds(120))
          .tcpKeepAlive(true)
          .build();

  /**
   * AWS Client configuration which increases the number of retries and alters the backoff strategy
   * to accommodate some network issues encountered in e2e test.
   */
  private static final ClientOverrideConfiguration defaultClientConfiguration =
      ClientOverrideConfiguration.builder()
          .retryPolicy(
              RetryPolicy.builder()
                  .numRetries(RETRY_POLICY_MAX_RETRIES)
                  .backoffStrategy(
                      EqualJitterBackoffStrategy.builder()
                          .maxBackoffTime(CLIENT_MAX_BACKOFF_TIME)
                          .baseDelay(CLIENT_BASE_DELAY)
                          .build())
                  .build())
          .build();

  // Override configuration that disables retries (the client does 3 by default)
  private static final ClientOverrideConfiguration NO_RETRIES_OVERRIDE =
      ClientOverrideConfiguration.builder()
          .retryPolicy(RetryPolicy.builder().numRetries(0).build())
          .build();

  private static final AwsCredentialsProvider createCredentialsProvider(
      LocalStackContainer localStack) {
    return StaticCredentialsProvider.create(
        AwsBasicCredentials.create(localStack.getAccessKey(), localStack.getSecretKey()));
  }

  private LocalStackAwsClientUtil() {}

  /** Returns an AWS KMS Client configured to use the specified localStack container. */
  public static KmsClient createKmsClient(LocalStackContainer localStack) {
    return KmsClient.builder()
        .endpointOverride(localStack.getEndpointOverride(LocalStackContainer.Service.KMS))
        .httpClient(defaultHttpClient)
        .credentialsProvider(createCredentialsProvider(localStack))
        .region(REGION)
        .overrideConfiguration(defaultClientConfiguration)
        .build();
  }

  /** Returns a CloudAeadSelector that is configured to use LocalStack to access cloud keys. */
  public static CloudAeadSelector createCloudAeadSelector(LocalStackContainer localStack) {
    return AwsTinkUtil.getKmsAeadSelector(
        createCredentialsProvider(localStack),
        defaultHttpClient,
        Optional.of(localStack.getEndpointOverride(LocalStackContainer.Service.KMS)));
  }

  /** Returns an AWS DynamoDB client configured to use the specified localStack container. */
  public static DynamoDbClient createDynamoDbClient(LocalStackContainer localStack) {
    return DynamoDbClient.builder()
        .endpointOverride(localStack.getEndpointOverride(LocalStackContainer.Service.DYNAMODB))
        .httpClient(defaultHttpClient)
        .credentialsProvider(createCredentialsProvider(localStack))
        .region(REGION)
        .overrideConfiguration(defaultClientConfiguration)
        .build();
  }

  /** Returns an AWS DynamoDB client configured to use the specified localStack container. */
  public static DynamoDbEnhancedClient createDynamoDbEnhancedClient(
      LocalStackContainer localStack) {
    return DynamoDbEnhancedClient.builder()
        .dynamoDbClient(createDynamoDbClient(localStack))
        .build();
  }

  /**
   * Returns an AWS DynamoDB Streams client configured to use the specified localStack container.
   */
  public static DynamoDbStreamsClient createDynamoDbStreamsClient(LocalStackContainer localStack) {
    return DynamoDbStreamsClient.builder()
        .endpointOverride(
            localStack.getEndpointOverride(LocalStackContainer.Service.DYNAMODB_STREAMS))
        .httpClient(defaultHttpClient)
        .credentialsProvider(createCredentialsProvider(localStack))
        .region(REGION)
        .overrideConfiguration(defaultClientConfiguration)
        .build();
  }

  /** Returns an AWS S3 client configured to use the specified localStack container. */
  public static S3Client createS3Client(LocalStackContainer localStack) {
    return S3Client.builder()
        .endpointOverride(localStack.getEndpointOverride(LocalStackContainer.Service.S3))
        .httpClient(defaultHttpClient)
        .credentialsProvider(createCredentialsProvider(localStack))
        .region(REGION)
        .overrideConfiguration(defaultClientConfiguration)
        .build();
  }

  /** Returns an AWS S3 Async client configured to use the specified localStack container. */
  public static S3AsyncClient createS3AsyncClient(LocalStackContainer localStack) {
    return S3AsyncClient.builder()
        .httpClient(defaultAsyncHttpClient)
        .endpointOverride(localStack.getEndpointOverride(LocalStackContainer.Service.S3))
        .credentialsProvider(createCredentialsProvider(localStack))
        .region(REGION)
        .overrideConfiguration(defaultClientConfiguration)
        .build();
  }

  /** Create an AWS SQS client configured to use the specified localStack container. */
  public static SqsClient createSqsClient(LocalStackContainer localStack) {
    return SqsClient.builder()
        .endpointOverride(localStack.getEndpointOverride(LocalStackContainer.Service.SQS))
        .httpClient(defaultHttpClient)
        .credentialsProvider(createCredentialsProvider(localStack))
        .region(REGION)
        .overrideConfiguration(defaultClientConfiguration)
        .build();
  }

  public static ApiGatewayClient createApiGatewayClient(LocalStackContainer localStack) {
    return ApiGatewayClient.builder()
        .endpointOverride(localStack.getEndpointOverride(LocalStackContainer.Service.API_GATEWAY))
        .httpClient(defaultHttpClient)
        .credentialsProvider(createCredentialsProvider(localStack))
        .region(REGION)
        .overrideConfiguration(defaultClientConfiguration)
        .build();
  }

  /**
   * Returns an AWS S3 client configured to use the specified localStack container. Does not retry
   * failed requests.
   */
  public static LambdaClient createLambdaClient(LocalStackContainer localStack) {
    return LambdaClient.builder()
        .endpointOverride(localStack.getEndpointOverride(LocalStackContainer.Service.LAMBDA))
        // longer HTTP client timeouts necessary to allow enough time for a lambda invocation to
        // reliably complete.
        .httpClient(lambdaHttpClient)
        .credentialsProvider(createCredentialsProvider(localStack))
        .region(REGION)
        .overrideConfiguration(NO_RETRIES_OVERRIDE)
        .build();
  }

  public static StsClient createStsClient(LocalStackContainer localStack) {
    return StsClient.builder()
        .endpointOverride(localStack.getEndpointOverride(LocalStackContainer.Service.STS))
        .httpClient(defaultHttpClient)
        .credentialsProvider(createCredentialsProvider(localStack))
        .region(REGION)
        .overrideConfiguration(defaultClientConfiguration)
        .build();
  }
}
