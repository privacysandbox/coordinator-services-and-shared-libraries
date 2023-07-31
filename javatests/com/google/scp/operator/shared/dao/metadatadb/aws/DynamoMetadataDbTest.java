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

package com.google.scp.operator.shared.dao.metadatadb.aws;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static com.google.scp.operator.protos.shared.backend.JobErrorCategoryProto.JobErrorCategory.DECRYPTION_ERROR;
import static com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus.FINISHED;
import static com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus.IN_PROGRESS;
import static com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus.RECEIVED;
import static com.google.scp.operator.protos.shared.backend.ReturnCodeProto.ReturnCode.SUCCESS;
import static com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb.MetadataDbDynamoClient;
import static com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb.MetadataDbDynamoTableName;
import static com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb.MetadataDbDynamoTtlDays;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Provides;
import com.google.protobuf.Timestamp;
import com.google.scp.operator.protos.shared.backend.CreateJobRequestProto.CreateJobRequest;
import com.google.scp.operator.protos.shared.backend.ErrorCountProto.ErrorCount;
import com.google.scp.operator.protos.shared.backend.ErrorSummaryProto.ErrorSummary;
import com.google.scp.operator.protos.shared.backend.JobKeyProto.JobKey;
import com.google.scp.operator.protos.shared.backend.ResultInfoProto.ResultInfo;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobKeyExistsException;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobMetadataConflictException;
import com.google.scp.operator.shared.testing.FakeClock;
import com.google.scp.shared.proto.ProtoUtil;
import com.google.scp.shared.testutils.aws.DynamoDbIntegrationTestModule;
import java.time.Clock;
import java.time.Instant;
import java.util.Optional;
import java.util.UUID;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.model.AttributeDefinition;
import software.amazon.awssdk.services.dynamodb.model.CreateTableRequest;
import software.amazon.awssdk.services.dynamodb.model.DeleteTableRequest;
import software.amazon.awssdk.services.dynamodb.model.KeySchemaElement;
import software.amazon.awssdk.services.dynamodb.model.KeyType;
import software.amazon.awssdk.services.dynamodb.model.ProvisionedThroughput;
import software.amazon.awssdk.services.dynamodb.model.ScalarAttributeType;
import software.amazon.awssdk.services.dynamodb.waiters.DynamoDbWaiter;

/**
 * Integration test which tests DynamoMetadataDb against Amazon's official local dynamodb container
 * (deployed locally using Docker).
 */
@RunWith(JUnit4.class)
public class DynamoMetadataDbTest {

  @Rule public final Acai acai = new Acai(TestEnv.class);
  @Inject DynamoMetadataDb dynamoMetadataDb;
  @Inject Clock clock;
  @Inject DynamoDbClient ddbClient;
  @Inject @MetadataDbDynamoTableName String tableName;
  JobKey jobKey;
  JobMetadata jobMetadata;
  ResultInfo resultInfo;

  /**
   * Performs an equality assertion on two JobMetadata objects and ignores version number and ttl
   */
  private static void assertJobMetadataEquals(JobMetadata actual, JobMetadata expected) {
    JobMetadata expectedNoVersionNumber =
        expected.toBuilder().clearRecordVersion().clearTtl().build();
    JobMetadata actualNoVerisonNumber = actual.toBuilder().clearRecordVersion().clearTtl().build();

    assertThat(actualNoVerisonNumber).isEqualTo(expectedNoVersionNumber);
  }

  @Before
  public void setUp() {
    createTable();

    String jobRequestId = UUID.randomUUID().toString();
    jobKey = JobKey.newBuilder().setJobRequestId(jobRequestId).build();

    jobMetadata =
        JobMetadata.newBuilder()
            .setJobKey(jobKey)
            .setRequestReceivedAt(ProtoUtil.toProtoTimestamp(Instant.now()))
            .setRequestUpdatedAt(ProtoUtil.toProtoTimestamp(Instant.now()))
            .setNumAttempts(0)
            .setJobStatus(RECEIVED)
            .setServerJobId(UUID.randomUUID().toString())
            .setCreateJobRequest(
                CreateJobRequest.newBuilder()
                    .setJobRequestId(jobRequestId)
                    .setInputDataBlobPrefix("foobar.avro")
                    .setInputDataBucketName("foo-bar")
                    .setOutputDataBlobPrefix("outputfile.avro")
                    .setOutputDataBucketName("bucket")
                    .setPostbackUrl("http://foo.com/bar")
                    .setAttributionReportTo("bar.com")
                    .setDebugPrivacyBudgetLimit(5)
                    .putAllJobParameters(
                        ImmutableMap.of(
                            "attribution_report_to",
                            "bar.com",
                            "output_domain_blob_prefix",
                            "outputfile.avro",
                            "output_domain_bucket_name",
                            "bucket",
                            "debug_privacy_budget_limit",
                            "5"))
                    .build())
            .build();

    resultInfo =
        ResultInfo.newBuilder()
            .setReturnCode(SUCCESS.name())
            .setReturnMessage("Success")
            .setErrorSummary(
                ErrorSummary.newBuilder()
                    .setNumReportsWithErrors(5)
                    .addAllErrorCounts(
                        ImmutableList.of(
                            ErrorCount.newBuilder()
                                .setCategory(DECRYPTION_ERROR.name())
                                .setCount(5L)
                                .build()))
                    .build())
            .setFinishedAt(ProtoUtil.toProtoTimestamp(Instant.parse("2021-01-01T00:00:00Z")))
            .build();
  }

  // TODO: abstract this into a testing module like {@link
  // com.google.scp.coordinator.keymanagement.testutils.KeyDbIntegrationTestModule}
  private void createTable() {
    CreateTableRequest request =
        CreateTableRequest.builder()
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
            .tableName(tableName)
            .build();
    ddbClient.createTable(request);
    DynamoDbWaiter waiter = ddbClient.waiter();
    waiter.waitUntilTableExists(r -> r.tableName(tableName));
  }

  @After
  public void teardown() {
    ddbClient.deleteTable(DeleteTableRequest.builder().tableName(tableName).build());
  }

  /** Test that getting a non-existent metadata item returns an empty optional */
  @Test
  public void testGetNonExistentReturnsEmpty() throws Exception {
    JobKey nonexistentJobKey =
        JobKey.newBuilder().setJobRequestId(UUID.randomUUID().toString()).build();

    Optional<JobMetadata> jobMetadata =
        dynamoMetadataDb.getJobMetadata(nonexistentJobKey.getJobRequestId());

    assertThat(jobMetadata).isEmpty();
  }

  /** Test that a metadata item can be inserted and read back */
  @Test
  public void testDynamoInsertAndGet() throws Exception {
    // No setup
    dynamoMetadataDb.insertJobMetadata(jobMetadata);
    Optional<JobMetadata> lookedUpJobMetadata =
        dynamoMetadataDb.getJobMetadata(jobKey.getJobRequestId());

    assertThat(lookedUpJobMetadata).isPresent();
    assertJobMetadataEquals(lookedUpJobMetadata.get(), jobMetadata);
    assertThat(lookedUpJobMetadata.get().getRequestProcessingStartedAt())
        .isEqualTo(Timestamp.getDefaultInstance());
  }

  /** Test that a metadata item with optional fields can be inserted and read back */
  @Test
  public void testDynamoInsertAndGetWithOptionalFields() throws Exception {

    JobMetadata modifiedJobMetadata =
        jobMetadata.toBuilder()
            .setJobStatus(FINISHED)
            .setResultInfo(resultInfo)
            .setRequestProcessingStartedAt(
                ProtoUtil.toProtoTimestamp(Instant.parse("2021-01-01T00:00:00Z")))
            .build();

    dynamoMetadataDb.insertJobMetadata(modifiedJobMetadata);
    Optional<JobMetadata> lookedUpJobMetadata =
        dynamoMetadataDb.getJobMetadata(jobKey.getJobRequestId());

    assertThat(lookedUpJobMetadata).isPresent();
    assertJobMetadataEquals(lookedUpJobMetadata.get(), modifiedJobMetadata);
  }

  /** Test that an exception is thrown when a duplicate metadata item is inserted */
  @Test
  public void testDynamoInsertNonUniqueThrows() throws Exception {
    // No setup

    dynamoMetadataDb.insertJobMetadata(jobMetadata);

    assertThrows(
        JobKeyExistsException.class, () -> dynamoMetadataDb.insertJobMetadata(jobMetadata));
  }

  /** Test that an exception is thrown when a duplicate metadata item is inserted */
  @Test
  public void testDynamoInserWithVersionThrows() throws Exception {
    JobMetadata jobMetadataWithVersion = jobMetadata.toBuilder().setRecordVersion(1).build();

    assertThrows(
        IllegalArgumentException.class,
        () -> dynamoMetadataDb.insertJobMetadata(jobMetadataWithVersion));
  }

  /** Test updating a metadata item */
  @Test
  public void testDynamoUpdateItem() throws Exception {
    // No setup

    dynamoMetadataDb.insertJobMetadata(jobMetadata);
    Optional<JobMetadata> lookedUpJobMetadata =
        dynamoMetadataDb.getJobMetadata(jobKey.getJobRequestId());
    JobMetadata modifiedJobMetadata =
        lookedUpJobMetadata.get().toBuilder()
            .setJobStatus(FINISHED)
            .setResultInfo(resultInfo)
            .build();
    dynamoMetadataDb.updateJobMetadata(modifiedJobMetadata);
    Optional<JobMetadata> secondLookedUpJobMetadata =
        dynamoMetadataDb.getJobMetadata(jobKey.getJobRequestId());

    assertThat(lookedUpJobMetadata).isPresent();
    assertJobMetadataEquals(
        secondLookedUpJobMetadata.get(),
        modifiedJobMetadata.toBuilder()
            .setRequestUpdatedAt(ProtoUtil.toProtoTimestamp(Instant.now(clock)))
            .build());
  }

  /** Test updating a non-existent metadata item throws an exception */
  @Test
  public void testDynamoUpdateNonexistentThrows() throws Exception {
    // No setup

    assertThrows(
        JobMetadataConflictException.class, () -> dynamoMetadataDb.updateJobMetadata(jobMetadata));
  }

  /** Test that updating with an outdated record version will throw an exception */
  @Test
  public void testDynamoUpdateWrongRecordVersionThrows() throws Exception {
    // No setup

    // Insert a job
    dynamoMetadataDb.insertJobMetadata(jobMetadata);
    // Now read it back and insert again
    Optional<JobMetadata> lookedUpJobMetadata =
        dynamoMetadataDb.getJobMetadata(jobKey.getJobRequestId());
    JobMetadata updatedJobMetadata =
        lookedUpJobMetadata.get().toBuilder().setJobStatus(IN_PROGRESS).build();
    dynamoMetadataDb.updateJobMetadata(updatedJobMetadata);

    // Now try to perform the update again using the JobMetadata that has an out-of-date
    // recordVersion (by simply performing the update again), this should throw an exception and
    // fail.
    assertThrows(
        JobMetadataConflictException.class,
        () -> dynamoMetadataDb.updateJobMetadata(updatedJobMetadata));
  }

  public static final class TestEnv extends AbstractModule {
    private static final String DYNAMO_TABLE_NAME = "DynamoMetadataDbTest";

    @Provides
    @MetadataDbDynamoClient
    public DynamoDbEnhancedClient provideEnhancedClient(DynamoDbClient client) {
      return DynamoDbEnhancedClient.builder().dynamoDbClient(client).build();
    }

    @Override
    public void configure() {
      bind(Clock.class).toInstance(new FakeClock());
      bind(String.class)
          .annotatedWith(MetadataDbDynamoTableName.class)
          .toInstance(DYNAMO_TABLE_NAME);

      bind(int.class).annotatedWith(MetadataDbDynamoTtlDays.class).toInstance(10);
      install(new DynamoDbIntegrationTestModule());
    }
  }
}
