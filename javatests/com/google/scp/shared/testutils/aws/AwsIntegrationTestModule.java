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

import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.createContainer;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.createDynamoDbClient;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.createLambdaClient;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.createS3Client;
import static org.testcontainers.containers.localstack.LocalStackContainer.Service.API_GATEWAY;
import static org.testcontainers.containers.localstack.LocalStackContainer.Service.DYNAMODB;
import static org.testcontainers.containers.localstack.LocalStackContainer.Service.KMS;
import static org.testcontainers.containers.localstack.LocalStackContainer.Service.LAMBDA;
import static org.testcontainers.containers.localstack.LocalStackContainer.Service.S3;
import static org.testcontainers.containers.localstack.LocalStackContainer.Service.SQS;
import static org.testcontainers.containers.localstack.LocalStackContainer.Service.STS;

import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import java.util.Optional;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.testcontainers.containers.Network;
import org.testcontainers.containers.localstack.LocalStackContainer;
import org.testcontainers.containers.output.Slf4jLogConsumer;
import org.testcontainers.containers.wait.strategy.Wait;
import software.amazon.awssdk.services.apigateway.ApiGatewayClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.kms.KmsClient;
import software.amazon.awssdk.services.lambda.LambdaClient;
import software.amazon.awssdk.services.s3.S3AsyncClient;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.sqs.SqsClient;
import software.amazon.awssdk.services.sts.StsClient;

/**
 * Module which starts a LocalStack container and provides AWS SDK clients configured to talk to
 * LocalStack. Provides clients for:
 *
 * <ul>
 *   <li>DynamoDB
 *   <li>S3
 *   <li>Lambda
 *   <li>KMS
 *   <li>SQS
 * </ul>
 */
public final class AwsIntegrationTestModule extends AbstractModule {
  // Use static logger name, printing the full class name is too verbose
  private static final Logger logger = LoggerFactory.getLogger("AwsIntegrationTestModule");
  // Hostname inside of the Docker network.
  private static final String networkAlias = "localstack";
  // Port that the LocalStack service listens on per https://github.com/localstack/localstack.
  private static final int internalPort = 4566;

  // Enables verbose localstack logging, enable with `bazel test --test_env=LOCALSTACK_DEBUG=true`.
  private static final boolean enableDebugLogs =
      Optional.ofNullable(System.getenv("LOCALSTACK_DEBUG"))
          .map(Boolean::parseBoolean)
          .orElse(false);

  private final Optional<Network> network;

  public AwsIntegrationTestModule() {
    this.network = Optional.empty();
  }

  public AwsIntegrationTestModule(Network network) {
    this.network = Optional.of(network);
  }

  /**
   * Returns the endpoint accessible from <b>within</b> the Docker network. This is not the same
   * endpoint as the <b>external</b> endpoint accessible from the host system.
   *
   * <p>The external endpoint can be obtained with a command like:
   *
   * <pre>localStack.getEndpointOverride(LocalStackContainer.Service.DYNAMODB)</pre>
   */
  public static String getInternalEndpoint() {
    return String.format("http://%s:%d", networkAlias, internalPort);
  }

  @Provides
  @Singleton
  public LocalStackContainer createLocalStackContainer() {
    var localStack = createContainer(S3, DYNAMODB, LAMBDA, KMS, API_GATEWAY, SQS, STS);
    if (network.isPresent()) {
      localStack.withEnv("LAMBDA_DOCKER_NETWORK", network.get().getId()).withNetwork(network.get());
    }
    localStack.withNetworkAliases(networkAlias);
    // Change KMS provider from default (moto). Moto provider has a bug that incorrectly reports
    // signature validation results.
    localStack.withEnv("KMS_PROVIDER", "local-kms");

    if (enableDebugLogs) {
      localStack.withEnv("DEBUG", "1");
      localStack.withEnv("LS_LOG", "debug");
    } else {
      localStack.withEnv("LS_LOG", "error");
    }

    // Start LocalStack before providing it because any clients which inject a LocalStackContainer
    // need it started in order to resolve the ports.
    localStack.start();
    localStack.waitingFor(Wait.forHealthcheck());

    // Log stream must be attached after container starts. withSeparateOutputStreams removes
    // redundant "STDOUT" message from logs.
    localStack.followOutput(new Slf4jLogConsumer(logger).withSeparateOutputStreams());

    return localStack;
  }

  @Provides
  public S3Client getS3Client(LocalStackContainer localstack) {
    return createS3Client(localstack);
  }

  @Provides
  public S3AsyncClient getS3AsyncClient(LocalStackContainer localstack) {
    return LocalStackAwsClientUtil.createS3AsyncClient(localstack);
  }

  @Provides
  public DynamoDbClient getDynamoDbClient(LocalStackContainer localstack) {
    return createDynamoDbClient(localstack);
  }

  @Provides
  public LambdaClient getLambdaClient(LocalStackContainer localstack) {
    return createLambdaClient(localstack);
  }

  @Provides
  public KmsClient getKmsClient(LocalStackContainer localStack) {
    return LocalStackAwsClientUtil.createKmsClient(localStack);
  }

  @Provides
  public ApiGatewayClient getApiGatewayClient(LocalStackContainer localStack) {
    return LocalStackAwsClientUtil.createApiGatewayClient(localStack);
  }

  @Provides
  public SqsClient getSqsClient(LocalStackContainer localStack) {
    return LocalStackAwsClientUtil.createSqsClient(localStack);
  }

  @Provides
  public StsClient getStsClient(LocalStackContainer localStack) {
    return LocalStackAwsClientUtil.createStsClient(localStack);
  }
}
