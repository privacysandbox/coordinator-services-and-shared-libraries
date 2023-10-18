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

import static com.google.common.collect.MoreCollectors.onlyElement;

import java.nio.file.Path;
import java.util.Collections;
import org.testcontainers.containers.localstack.LocalStackContainer;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.model.AttributeDefinition;
import software.amazon.awssdk.services.dynamodb.model.CreateTableRequest;
import software.amazon.awssdk.services.dynamodb.model.DeleteTableRequest;
import software.amazon.awssdk.services.dynamodb.model.KeySchemaElement;
import software.amazon.awssdk.services.dynamodb.model.KeyType;
import software.amazon.awssdk.services.dynamodb.model.ProvisionedThroughput;
import software.amazon.awssdk.services.dynamodb.model.ScalarAttributeType;
import software.amazon.awssdk.services.dynamodb.model.StreamSpecification;
import software.amazon.awssdk.services.dynamodb.model.StreamViewType;
import software.amazon.awssdk.services.dynamodb.streams.DynamoDbStreamsClient;
import software.amazon.awssdk.services.dynamodb.waiters.DynamoDbWaiter;
import software.amazon.awssdk.services.kms.KmsClient;
import software.amazon.awssdk.services.lambda.LambdaClient;
import software.amazon.awssdk.services.lambda.model.CreateEventSourceMappingRequest;
import software.amazon.awssdk.services.lambda.model.EventSourcePosition;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.sqs.SqsClient;
import software.amazon.awssdk.services.sqs.model.CreateQueueRequest;
import software.amazon.awssdk.services.sqs.model.CreateQueueResponse;
import software.amazon.awssdk.services.sqs.model.GetQueueAttributesRequest;
import software.amazon.awssdk.services.sqs.model.GetQueueAttributesResponse;
import software.amazon.awssdk.services.sqs.model.ListQueuesRequest;
import software.amazon.awssdk.services.sqs.model.QueueAttributeName;
import software.amazon.awssdk.services.sqs.model.SetQueueAttributesRequest;

/** Helper class providing methods to test AWS features locally. */
public final class AwsHermeticTestHelper {

  private static final String ENDPOINT = "http://localhost:4566";
  private static final String QUEUE_NAME = "SqsHermeticTest";
  private static final String QUEUE_URL = ENDPOINT.concat("/000000000000/").concat(QUEUE_NAME);
  private static final String BUCKET_NAME = "bucketname";
  private static final Region REGION = Region.US_EAST_1;
  private static final String DYNAMO_DB_TABLE_NAME = "HermeticTest";

  private AwsHermeticTestHelper() {}

  /**
   * Return LocalStackContainer for ClassRule to locally test AWS features via a Docker container.
   *
   * @deprecated use {@link AwsIntegrationTestUtil#createContainer(LocalStackContainer.Service...)}.
   */
  @Deprecated
  public static LocalStackContainer createContainer(LocalStackContainer.Service... services) {
    return AwsIntegrationTestUtil.createContainer(services).withEnv("DEBUG", "1");
  }

  /** Return the URL of the SQS Queue using the default queue name */
  public static String getSqsQueueUrlDefaultName() {
    return QUEUE_URL;
  }

  /** Return BucketName set on localstack container. */
  public static String getBucketName() {
    return BUCKET_NAME;
  }

  /** Return the name of the DynamoDb table used */
  public static String getDynamoDbTableName() {
    return DYNAMO_DB_TABLE_NAME;
  }

  /** Return the region used */
  public static Region getRegion() {
    return REGION;
  }

  /**
   * Creates a symmetric AWS KMS key and returns its key ID.
   *
   * @dperecated use {@link AwsIntegrationTestUtil.createKmsKey}
   */
  @Deprecated
  public static String createKey(KmsClient kmsClient) {
    return AwsIntegrationTestUtil.createKmsKey(kmsClient);
  }

  /**
   * Create an SqsClient that will connect to the localstack container. This method does not create
   * a queue, another method must be called to create the queue.
   *
   * @deprecated use {@link LocalStackAwsClientUtil.createSqsClient}.
   */
  @Deprecated
  public static SqsClient createSqsClient(LocalStackContainer localstack) {
    return LocalStackAwsClientUtil.createSqsClient(localstack);
  }

  /**
   * Creates a normal SQS queue with the given name
   *
   * @return the url of the queue created
   */
  public static String createNormalQueueWithName(SqsClient sqsClient, String queueName) {
    CreateQueueRequest createQueueRequest =
        CreateQueueRequest.builder().queueName(queueName).build();
    CreateQueueResponse createQueueResponse = sqsClient.createQueue(createQueueRequest);
    return createQueueResponse.queueUrl();
  }

  /** Creates a SQS queue with the default name */
  public static void createSqsQueueDefaultName(SqsClient sqsClient) {
    CreateQueueRequest createQueueRequest =
        CreateQueueRequest.builder().queueName(QUEUE_NAME).build();
    sqsClient.createQueue(createQueueRequest);
    SetQueueAttributesRequest setQueueAttributesRequest =
        SetQueueAttributesRequest.builder().queueUrl(QUEUE_URL).build();
    sqsClient.setQueueAttributes(setQueueAttributesRequest);
  }

  /** Looks up the URL for the SQS queue with the name provided */
  public static String getSqsQueueUrlWithName(SqsClient sqsClient, String queueName) {
    ListQueuesRequest listQueuesRequest =
        ListQueuesRequest.builder().queueNamePrefix(queueName).build();
    return sqsClient.listQueues(listQueuesRequest).queueUrls().stream().collect(onlyElement());
  }

  /** Looks up the ARN for the SQS queue with the name provided */
  public static String getSqsQueueArnWithUrl(SqsClient sqsClient, String queueUrl) {
    GetQueueAttributesRequest getQueueAttributesRequest =
        GetQueueAttributesRequest.builder()
            .queueUrl(queueUrl)
            .attributeNames(QueueAttributeName.QUEUE_ARN)
            .build();
    GetQueueAttributesResponse getQueueAttributesResponse =
        sqsClient.getQueueAttributes(getQueueAttributesRequest);
    return getQueueAttributesResponse.attributes().get(QueueAttributeName.QUEUE_ARN);
  }

  /**
   * Return AWS S3Client with a bucket BUCKET_NAME. This client interacts with a localstack docker
   * container allowing tests to be hermetic.
   *
   * @deprecated use {@link LocalStackAwsClientUtil.createS3Client} and manually create the
   *     necessary bucket.
   */
  @Deprecated
  public static S3Client createS3Client(LocalStackContainer localstack) {
    S3Client s3Client = LocalStackAwsClientUtil.createS3Client(localstack);
    createBucket(s3Client);
    return s3Client;
  }

  /** Creates the standard S3 bucket used in conjunction with the AwsHermeticTestHelper */
  public static void createBucket(S3Client s3Client) {
    s3Client.createBucket(b -> b.bucket(BUCKET_NAME));
  }

  /**
   * Provide KmsClient that uses local KMS services instead of AWS KMS services.
   *
   * @deprecated use {@link LocalStackAwsClientUtil.createKmsClient} instead.
   */
  @Deprecated
  public static KmsClient createKmsClient(LocalStackContainer localstack) {
    return LocalStackAwsClientUtil.createKmsClient(localstack);
  }

  /** Provide a DynamoDbClient that uses the localstack container */
  public static DynamoDbClient createDynamoDbClientAndTable(LocalStackContainer localstack) {
    // Create the client
    DynamoDbClient dynamoDbClient = LocalStackAwsClientUtil.createDynamoDbClient(localstack);
    createDynamoDbTable(dynamoDbClient);
    return dynamoDbClient;
  }

  public static void createDynamoDbTable(DynamoDbClient dynamoDbClient) {
    // Attempt to delete the table before creating it to take care of potentially leaked state
    try {
      dynamoDbClient.deleteTable(
          DeleteTableRequest.builder().tableName(DYNAMO_DB_TABLE_NAME).build());
    } catch (Exception e) {
    }
    // Create the table with a stream
    CreateTableRequest createTableRequest =
        CreateTableRequest.builder()
            .tableName(DYNAMO_DB_TABLE_NAME)
            .attributeDefinitions(
                AttributeDefinition.builder()
                    .attributeName("JobKey")
                    .attributeType(ScalarAttributeType.S)
                    .build())
            .keySchema(
                KeySchemaElement.builder().attributeName("JobKey").keyType(KeyType.HASH).build())
            .provisionedThroughput(
                ProvisionedThroughput.builder()
                    .readCapacityUnits(10L)
                    .writeCapacityUnits(10L)
                    .build())
            .streamSpecification(
                StreamSpecification.builder()
                    .streamEnabled(true)
                    .streamViewType(StreamViewType.NEW_AND_OLD_IMAGES)
                    .build())
            .build();
    dynamoDbClient.createTable(createTableRequest);

    // Wait for the table to be ready, then return the client
    DynamoDbWaiter waiter = dynamoDbClient.waiter();
    waiter.waitUntilTableExists(requestBuilder -> requestBuilder.tableName(DYNAMO_DB_TABLE_NAME));
  }

  /**
   * Provide a DynamoDbEnhancedClient that uses the localstack container
   *
   * @deprecated use {@link LocalStackAwsClientUtil.createDynamoDbEnhancedClient}.
   */
  @Deprecated
  public static DynamoDbEnhancedClient createDynamoDbEnhancedClient(
      LocalStackContainer localStack) {
    return LocalStackAwsClientUtil.createDynamoDbEnhancedClient(localStack);
  }

  /**
   * Provide a DynamoDbStreamsClient that uses the localstack container.
   *
   * <p>Note: this does not actually create the stream, the stream is created when
   * createDynamoDbClient is called
   *
   * @deprecated use {@link LocalStackAwsClientUtil.createDynamoDbStreamsClient} instead.
   */
  @Deprecated
  public static DynamoDbStreamsClient createDynamoDbStreamsClient(LocalStackContainer localstack) {
    return LocalStackAwsClientUtil.createDynamoDbStreamsClient(localstack);
  }

  /**
   * Creates a lambda client configured to use the localstack container.
   *
   * @deprecated use {@link LocalStackAwsClientUtil.createLambdaClient}.
   */
  @Deprecated
  public static LambdaClient createLambdaClient(LocalStackContainer localstack) {
    return LocalStackAwsClientUtil.createLambdaClient(localstack);
  }

  /**
   * Adds a JAR for a lambda to s3 to later be used.
   *
   * @deprecated use {@link AwsIntegrationTestUtil.uploadLambdaCode}
   */
  @Deprecated
  public static void uploadLambdaCode(S3Client s3Client, String s3Key, Path pathToJar) {
    AwsIntegrationTestUtil.uploadLambdaCode(s3Client, s3Key, pathToJar);
  }

  /**
   * Adds a lambda handler class to the localstack container
   *
   * @param lambdaClient a lambda client that is connected to the localstack container
   * @param functionName the name to identify the lambda function
   * @param handlerClass the fully qualified classname of the handler class
   * @param handlerJarS3Key the s3 key for the lambda jar
   * @return the ARN of the lambda function created
   * @deprecated use {@link AwsIntegrationTestUtil.addLambdaHelper}
   */
  @Deprecated
  public static String addLambdaHandler(
      LambdaClient lambdaClient, String functionName, String handlerClass, String handlerJarS3Key) {
    var environmentVariables = Collections.<String, String>emptyMap();
    return AwsIntegrationTestUtil.addLambdaHandler(
        lambdaClient, functionName, handlerClass, handlerJarS3Key, environmentVariables);
  }

  /**
   * Creates an event source mapping for the lambda provided
   *
   * @param lambdaClient a lambda client that is connected to the localstack container
   * @param functionName the name of the lambda function
   * @param eventSourceArn the arn of some event source (e.g. SQS)
   */
  public static void addLambdaEventSourceMapping(
      LambdaClient lambdaClient, String functionName, String eventSourceArn) {
    CreateEventSourceMappingRequest createEventSourceMappingRequest =
        CreateEventSourceMappingRequest.builder()
            .functionName(functionName)
            .eventSourceArn(eventSourceArn)
            .startingPosition(EventSourcePosition.LATEST)
            .build();
    lambdaClient.createEventSourceMapping(createEventSourceMappingRequest);
  }
}
