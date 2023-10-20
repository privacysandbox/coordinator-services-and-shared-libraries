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

import com.google.acai.Acai;
import com.google.auto.service.AutoService;
import com.google.common.base.Supplier;
import com.google.common.base.Suppliers;
import com.google.common.collect.ImmutableList;
import com.google.common.truth.Correspondence;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.frontend.service.aws.model.DdbStreamBatchInfo;
import com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.operator.shared.injection.modules.BaseAwsClientsModule;
import com.google.scp.operator.shared.model.BackendModelUtil;
import com.google.scp.operator.shared.testing.HermeticAwsClientsModule;
import com.google.scp.operator.shared.testing.IntegrationTestDataModule;
import com.google.scp.shared.testutils.aws.AwsHermeticTestHelper;
import java.util.Collection;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.testcontainers.containers.localstack.LocalStackContainer;
import org.testcontainers.containers.localstack.LocalStackContainer.Service;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.model.DescribeStreamRequest;
import software.amazon.awssdk.services.dynamodb.model.DescribeStreamResponse;
import software.amazon.awssdk.services.dynamodb.model.ListStreamsRequest;
import software.amazon.awssdk.services.dynamodb.model.ListStreamsResponse;
import software.amazon.awssdk.services.dynamodb.streams.DynamoDbStreamsClient;

@RunWith(JUnit4.class)
public class DdbStreamJobMetadataLookupTest {

  private static final Supplier<LocalStackContainer> memoizedContainerSupplier =
      Suppliers.memoize(
          () -> AwsHermeticTestHelper.createContainer(Service.DYNAMODB, Service.DYNAMODB_STREAMS));
  @ClassRule public static LocalStackContainer localstack = memoizedContainerSupplier.get();
  @Rule public Acai acai = new Acai(TestEnv.class);
  @Inject DynamoMetadataDb dynamoMetadataDb;
  @Inject DynamoDbStreamsClient dynamoDbStreamsClient;
  @Inject DdbStreamJobMetadataLookup ddbStreamJobMetadataLookup;
  @Inject DynamoDbClient dynamoDbClient;
  @Inject DynamoDbEnhancedClient dynamoDbEnhancedClient;
  ImmutableList<JobMetadata> jobMetadatas;
  ImmutableList<DdbStreamBatchInfo> streamBatchInfos;

  @Before
  public void setUp() throws Exception {
    AwsHermeticTestHelper.createDynamoDbTable(dynamoDbClient);
    jobMetadatas =
        ImmutableList.of(
            JobGenerator.createFakeJobMetadata("wee").toBuilder()
                .setJobStatus(JobStatus.RECEIVED)
                .clearResultInfo()
                .clearRecordVersion()
                .build(),
            JobGenerator.createFakeJobMetadata("bar").toBuilder()
                .clearResultInfo()
                .clearRecordVersion()
                .build());

    for (JobMetadata jobMetadata : jobMetadatas) {
      dynamoMetadataDb.insertJobMetadata(jobMetadata);
    }

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
    // Store all the shards since streams can have multiple shards
    streamBatchInfos =
        describeStreamResponse.streamDescription().shards().stream()
            .map(
                shard ->
                    DdbStreamBatchInfo.builder()
                        .shardId(shard.shardId())
                        .streamArn(streamArn)
                        .startSequenceNumber(shard.sequenceNumberRange().startingSequenceNumber())
                        .batchSize(jobMetadatas.size())
                        .build())
            .collect(toImmutableList());
  }

  /** Simple test to retrieve metadata entries from the stream */
  @Test
  public void testLookup() {
    // No setup

    // Lookup records from all the stream shards
    ImmutableList<JobMetadata> retrievedJobMetadatas =
        streamBatchInfos.stream()
            .map(streamBatchInfo -> ddbStreamJobMetadataLookup.lookupInStream(streamBatchInfo))
            .flatMap(Collection::stream)
            .collect(toImmutableList());

    // Assert that the JobMetadatas retrieved are the expected ones, comparing using
    // JobMetadata::equalsIgnoreRecordVersion
    assertThat(retrievedJobMetadatas)
        .comparingElementsUsing(
            Correspondence.from(
                BackendModelUtil::equalsIgnoreDbFields, "equalsIgnoreRecordVersion"))
        .containsExactlyElementsIn(jobMetadatas);
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
