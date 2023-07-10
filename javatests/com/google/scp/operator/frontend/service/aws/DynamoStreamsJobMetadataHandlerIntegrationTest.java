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

import static com.google.common.truth.Truth.assertThat;

import com.google.acai.Acai;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.dao.metadatadb.aws.DynamoMetadataDb;
import com.google.scp.operator.dao.metadatadb.aws.model.DynamoJobMetadata;
import com.google.scp.operator.dao.metadatadb.model.JobMetadata;
import com.google.scp.operator.dao.metadatadb.testing.DynamodbStreamRecordJobMetadataFakeFactory;
import com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus;
import com.google.scp.operator.shared.testing.Copy;
import com.google.scp.operator.shared.testing.DynamoStreamsIntegrationTestModule;
import com.google.scp.operator.shared.testing.Source;
import java.util.Optional;
import java.util.logging.Logger;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbEnhancedClient;
import software.amazon.awssdk.enhanced.dynamodb.DynamoDbTable;
import software.amazon.awssdk.enhanced.dynamodb.Key;
import software.amazon.awssdk.enhanced.dynamodb.TableSchema;

/**
 * This is a manual test that requires two dynamo db tables named jobmetadatastreams and
 * copyjobmetadatastreams. Both tables have a primary key {S:JobKey}. Any changes to the
 * jobmetadatastreams table triggers a streams handler whose lambda function handler is {@link
 * DynamoStreamsJobMetadataHandler ::handleRequest}. This function has the {@link
 * com.google.scp.operator.shared.testing.CopyJobHandler} registered with Guice to the collection of
 * {@link com.google.scp.frontend.services.JobMetadataChangeHandler}s. CopyJobHandler only handles
 * JobMetadata if the status is JobStatus.RECEIVED or JobStatus.IN_PROGRESS . CopyJobHandler copies
 * the handled JobMetadata to the copyjobmetadatastreams table.
 */
@RunWith(JUnit4.class)
public class DynamoStreamsJobMetadataHandlerIntegrationTest {

  private static final Logger logger = Logger.getLogger("");
  @Rule public final Acai acai = new Acai(TestEnv.class);
  @Inject @Source DynamoMetadataDb sourceDynamoMetadataDb;
  @Inject @Copy DynamoMetadataDb copyDynamoMetadataDb;
  @Inject DynamoDbEnhancedClient dynamoDbEnhancedClient;
  JobMetadata jobMetadata;

  @Before
  public void setUp() {
    logger.info("Calling setup");
    jobMetadata = DynamodbStreamRecordJobMetadataFakeFactory.createFakeJobMetadata("1");
  }

  // Delete from both tables before just in case data is already present
  @Before
  @After
  public void tearDown() {
    logger.info("Calling teardown");
    deleteFromTable("copyjobmetadatastreams");
    deleteFromTable("jobmetadatastreams");
  }

  private void deleteFromTable(String tableName) {
    DynamoDbTable<DynamoJobMetadata> sourceTable =
        dynamoDbEnhancedClient.table(
            tableName, TableSchema.fromImmutableClass(DynamoJobMetadata.class));
    sourceTable.deleteItem(
        Key.builder().partitionValue(jobMetadata.jobKey().toKeyString()).build());
  }

  @Test
  public void streamsHandlerCalled_whenJobMetadataInserted() throws Exception {
    sourceDynamoMetadataDb.insertJobMetadata(jobMetadata);

    Thread.sleep(2000);

    Optional<JobMetadata> copiedJob =
        copyDynamoMetadataDb.getJobMetadata(jobMetadata.jobKey().toKeyString());

    assertThat(copiedJob.get()).isEqualTo(jobMetadata);
  }

  @Test
  public void streamsHandlerCalled_whenJobMetadataUpdated() throws Exception {
    sourceDynamoMetadataDb.insertJobMetadata(jobMetadata);

    Thread.sleep(2000);

    jobMetadata = jobMetadata.toBuilder().setJobStatus(JobStatus.RECEIVED).build();
    deleteFromTable("copyjobmetadatastreams");
    sourceDynamoMetadataDb.updateJobMetadata(jobMetadata);

    Thread.sleep(2000);

    Optional<JobMetadata> copiedJob =
        copyDynamoMetadataDb.getJobMetadata(jobMetadata.jobKey().toKeyString());

    assertThat(copiedJob.get()).isEqualTo(jobMetadata);
  }

  @Test
  public void streamsHandlerNotCalled_whenNoChangeHandlerRegisteredForUpdate() throws Exception {
    jobMetadata = jobMetadata.toBuilder().setJobStatus(JobStatus.FINISHED).build();
    sourceDynamoMetadataDb.insertJobMetadata(jobMetadata);

    Thread.sleep(2000);

    Optional<JobMetadata> copiedJob =
        copyDynamoMetadataDb.getJobMetadata(jobMetadata.jobKey().toKeyString());

    assertThat(copiedJob.isEmpty()).isTrue();
  }

  public static final class TestEnv extends AbstractModule {

    @Override
    public void configure() {
      install(new DynamoStreamsIntegrationTestModule());
    }
  }
}
