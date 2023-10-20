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

package com.google.scp.operator.frontend.service.aws;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.common.collect.MoreCollectors.onlyElement;
import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil.getLocalStackHostname;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.acai.Acai;
import com.google.auto.service.AutoService;
import com.google.common.base.Supplier;
import com.google.common.base.Suppliers;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.frontend.service.aws.changehandler.MarkJobFailedToEnqueueHandler;
import com.google.scp.operator.frontend.service.aws.model.DdbStreamBatchInfo;
import com.google.scp.operator.protos.shared.backend.ErrorSummaryProto.ErrorSummary;
import com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus;
import com.google.scp.operator.protos.shared.backend.ResultInfoProto.ResultInfo;
import com.google.scp.operator.protos.shared.backend.ReturnCodeProto.ReturnCode;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.operator.shared.injection.modules.BaseAwsClientsModule;
import com.google.scp.operator.shared.injection.modules.EnvironmentVariables;
import com.google.scp.operator.shared.testing.HermeticAwsClientsModule;
import com.google.scp.operator.shared.testing.IntegrationTestDataModule;
import com.google.scp.shared.proto.ProtoUtil;
import com.google.scp.shared.testutils.aws.AwsHermeticTestHelper;
import com.google.scp.shared.testutils.aws.AwsIntegrationTestUtil;
import java.nio.file.Path;
import java.time.Instant;
import java.util.Optional;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.testcontainers.containers.localstack.LocalStackContainer;
import org.testcontainers.containers.localstack.LocalStackContainer.Service;
import software.amazon.awssdk.services.dynamodb.model.DescribeStreamRequest;
import software.amazon.awssdk.services.dynamodb.model.DescribeStreamResponse;
import software.amazon.awssdk.services.dynamodb.model.ListStreamsRequest;
import software.amazon.awssdk.services.dynamodb.model.ListStreamsResponse;
import software.amazon.awssdk.services.dynamodb.streams.DynamoDbStreamsClient;
import software.amazon.awssdk.services.lambda.LambdaClient;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.sqs.SqsClient;
import software.amazon.awssdk.services.sqs.model.ReceiveMessageRequest;
import software.amazon.awssdk.services.sqs.model.SendMessageRequest;

/**
 * Loads the AwsFailedJobQueueWriteCleanup lambda JAR into localstack and triggers it to test the
 * cleanup operation
 */
@RunWith(JUnit4.class)
public class AwsFailedJobQueueWriteCleanupTest {

  private static final String HANDLER_CLASS =
      "com.google.scp.operator.frontend.service.aws.AwsFailedJobQueueWriteCleanup";
  private static final String LAMBDA_JAR_KEY =
      "AwsFailedJobQueueWriteCleanupTest/AwsFailedJobQueueWriteCleanup.jar";
  private static final String LAMBDA_NAME = "AwsFailedJobQueueWriteCleanup";
  private static final int EXECUTION_WAIT_PERIOD_MS = 2 * 60 * 1000; // 2 minutes
  private static final Instant FIXED_TIME = Instant.parse("2021-01-01T00:00:00Z");
  private static final String SQS_QUEUE_NAME = "CleanupQueue";
  // Class rule for hermetic testing.
  private static final Supplier<LocalStackContainer> memoizedContainerSupplier =
      Suppliers.memoize(
          () ->
              AwsHermeticTestHelper.createContainer(
                  Service.SQS,
                  Service.S3,
                  Service.DYNAMODB,
                  Service.DYNAMODB_STREAMS,
                  Service.LAMBDA));
  @ClassRule public static LocalStackContainer localstack = memoizedContainerSupplier.get();
  @Rule public Acai acai = new Acai(TestEnv.class);
  @Inject DynamoMetadataDb dynamoMetadataDb;
  @Inject SqsClient sqsClient;
  @Inject DynamoDbStreamsClient dynamoDbStreamsClient;
  @Inject ObjectMapper objectMapper;

  JobMetadata jobMetadata;
  JobMetadata jobMetadataReceivedByWorker;
  ImmutableList<DdbStreamBatchInfo> streamBatchInfos;

  @BeforeClass
  public static void setUpClass() {
    SqsClient sqsClient = AwsHermeticTestHelper.createSqsClient(localstack);
    String sqsUrl = AwsHermeticTestHelper.createNormalQueueWithName(sqsClient, SQS_QUEUE_NAME);
    String sqsArn = AwsHermeticTestHelper.getSqsQueueArnWithUrl(sqsClient, sqsUrl);

    String handlerJarPath = System.getenv("LAMBDA_LOCATION");
    S3Client s3Client = AwsHermeticTestHelper.createS3Client(localstack);
    AwsHermeticTestHelper.uploadLambdaCode(s3Client, LAMBDA_JAR_KEY, Path.of(handlerJarPath));
    LambdaClient lambdaClient = AwsHermeticTestHelper.createLambdaClient(localstack);
    String functionArn =
        AwsIntegrationTestUtil.addLambdaHandler(
            lambdaClient,
            LAMBDA_NAME,
            HANDLER_CLASS,
            LAMBDA_JAR_KEY,
            ImmutableMap.of(
                EnvironmentVariables.AWS_ENDPOINT_URL_ENV_VAR,
                String.format("http://%s:4566", getLocalStackHostname(localstack))));

    AwsHermeticTestHelper.addLambdaEventSourceMapping(lambdaClient, functionArn, sqsArn);
    AwsHermeticTestHelper.createDynamoDbClientAndTable(localstack);
  }

  @AfterClass
  public static void printContainerLogs() {
    System.out.println("== Logs from localstack container ==");
    System.out.println(localstack.getLogs());
  }

  @Before
  public void setUp() throws Exception {
    // Job that will be cleaned up
    jobMetadata =
        JobGenerator.createFakeJobMetadata("foo").toBuilder()
            .setJobStatus(JobStatus.RECEIVED)
            .clearResultInfo()
            .clearRecordVersion()
            .build();
    // Job that won't be cleaned up since it was marked as IN_PROGRESS
    jobMetadataReceivedByWorker =
        JobGenerator.createFakeJobMetadata("bar").toBuilder()
            .setJobStatus(JobStatus.RECEIVED)
            .clearResultInfo()
            .clearRecordVersion()
            .build();

    dynamoMetadataDb.insertJobMetadata(jobMetadata);
    dynamoMetadataDb.insertJobMetadata(jobMetadataReceivedByWorker);

    // Now update one of the jobs to IN_PROGRESS (to simulate the worker starting on it)
    dynamoMetadataDb.updateJobMetadata(
        dynamoMetadataDb
            .getJobMetadata(jobMetadataReceivedByWorker.getJobKey().getJobRequestId())
            .get()
            .toBuilder()
            .setJobStatus(JobStatus.IN_PROGRESS)
            .build());

    // Use the DynamoDb Streams API to lookup the ARN, ShardId, and startSequenceNumber to populate
    // the ddbStreamBatchInfo
    ListStreamsResponse listStreamsResponse =
        dynamoDbStreamsClient.listStreams(
            ListStreamsRequest.builder()
                .tableName(AwsHermeticTestHelper.getDynamoDbTableName())
                .build());
    String streamArn = listStreamsResponse.streams().stream().collect(onlyElement()).streamArn();
    DescribeStreamResponse describeStreamResponse =
        dynamoDbStreamsClient.describeStream(
            DescribeStreamRequest.builder().streamArn(streamArn).build());
    // Store all the shards since streams can have multiple shards (typically there is only 1,
    // however)
    streamBatchInfos =
        describeStreamResponse.streamDescription().shards().stream()
            .map(
                shard ->
                    DdbStreamBatchInfo.builder()
                        .shardId(shard.shardId())
                        .streamArn(streamArn)
                        .startSequenceNumber(shard.sequenceNumberRange().startingSequenceNumber())
                        .batchSize(2)
                        .build())
            .collect(toImmutableList());
  }

  /**
   * Test that the lambda handler cleans up the job in state RECEIVED and doesn't clean up the job
   * in state IN_PROGRESS.
   */
  @Test
  public void testHandlerOnlyMarksReceivedJobFailed() throws Exception {
    ResultInfo expectedResultInfo =
        ResultInfo.newBuilder()
            .setReturnCode(ReturnCode.INTERNAL_ERROR.name())
            .setReturnMessage(MarkJobFailedToEnqueueHandler.RETURN_MESSAGE)
            .setErrorSummary(ErrorSummary.getDefaultInstance())
            .setFinishedAt(ProtoUtil.toProtoTimestamp(FIXED_TIME))
            .build();
    String queueUrl = AwsHermeticTestHelper.getSqsQueueUrlWithName(sqsClient, SQS_QUEUE_NAME);

    // Put messages on SQS for all stream batch infos
    for (DdbStreamBatchInfo streamBatchInfo : streamBatchInfos) {
      String messageBody =
          String.format(
              "{\"DDBStreamBatchInfo\": %s }", objectMapper.writeValueAsString(streamBatchInfo));
      sqsClient.sendMessage(
          SendMessageRequest.builder().queueUrl(queueUrl).messageBody(messageBody).build());
    }

    // Wait for the lambda to be triggered and handler the messages
    Thread.sleep(EXECUTION_WAIT_PERIOD_MS);

    // Check that the first job is now cleaned up (marked as FINISHED)
    Optional<JobMetadata> jobMetadataAfterCleanup =
        dynamoMetadataDb.getJobMetadata(jobMetadata.getJobKey().getJobRequestId());
    assertThat(jobMetadataAfterCleanup).isPresent();
    assertThat(jobMetadataAfterCleanup.get().getJobStatus()).isEqualTo(JobStatus.FINISHED);
    // Check that the ResultInfo says its failed due to an internal issue, ignoring the finishedAt
    // timestamp
    ResultInfo actualResultInfo =
        jobMetadataAfterCleanup.get().getResultInfo().toBuilder()
            .setFinishedAt(ProtoUtil.toProtoTimestamp(FIXED_TIME))
            .build();
    assertThat(actualResultInfo).isEqualTo(expectedResultInfo);

    // Check that the second job was not cleaned up (still IN_PROGRESS)
    assertThat(
            dynamoMetadataDb
                .getJobMetadata(jobMetadataReceivedByWorker.getJobKey().getJobRequestId())
                .get()
                .getJobStatus())
        .isEqualTo(JobStatus.IN_PROGRESS);

    // Check that all messages have been processed from the queue
    boolean queueHasMessages =
        sqsClient
            .receiveMessage(ReceiveMessageRequest.builder().queueUrl(queueUrl).build())
            .hasMessages();
    assertThat(queueHasMessages).isFalse();
  }

  public static final class TestEnv extends AbstractModule {

    public void configure() {
      install(
          new IntegrationTestDataModule() {
            @Override
            public String getJobMetadataTableName() {
              return AwsHermeticTestHelper.getDynamoDbTableName();
            }
          });
    }
  }

  /**
   * Creates AWS clients that are compatible with the AwsHermeticTestHelper. This module is loaded
   * dynamically.
   */
  @AutoService(BaseAwsClientsModule.class)
  public static final class AwsClientsModule extends HermeticAwsClientsModule {

    @Override
    protected LocalStackContainer getLocalStack() {
      return localstack;
    }
  }
}
