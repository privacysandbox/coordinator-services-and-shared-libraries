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

package com.google.scp.operator.shared.dao.metadatadb.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static com.google.scp.operator.protos.shared.backend.JobErrorCategoryProto.JobErrorCategory.DECRYPTION_ERROR;
import static com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus.FINISHED;
import static com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus.RECEIVED;
import static com.google.scp.operator.protos.shared.backend.ReturnCodeProto.ReturnCode.SUCCESS;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.inject.Inject;
import com.google.scp.operator.protos.shared.backend.ErrorCountProto.ErrorCount;
import com.google.scp.operator.protos.shared.backend.ErrorSummaryProto.ErrorSummary;
import com.google.scp.operator.protos.shared.backend.JobKeyProto.JobKey;
import com.google.scp.operator.protos.shared.backend.RequestInfoProto.RequestInfo;
import com.google.scp.operator.protos.shared.backend.ResultInfoProto.ResultInfo;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobKeyExistsException;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobMetadataDbException;
import com.google.scp.shared.proto.ProtoUtil;
import java.time.Instant;
import java.util.Optional;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class SpannerMetadataDbTest {

  @Rule public final Acai acai = new Acai(SpannerMetadataDbTestModule.class);

  @Inject SpannerMetadataDb spannerMetadataDb;

  JobKey jobKey;
  JobMetadata jobMetadata;

  @Before
  public void setUp() {
    String jobRequestId = UUID.randomUUID().toString();
    jobKey = JobKey.newBuilder().setJobRequestId(jobRequestId).build();

    jobMetadata =
        JobMetadata.newBuilder()
            .setJobKey(jobKey)
            .setRequestReceivedAt(ProtoUtil.toProtoTimestamp(Instant.parse("2020-01-01T00:00:00Z")))
            .setRequestUpdatedAt(ProtoUtil.toProtoTimestamp(Instant.parse("2020-01-01T00:00:00Z")))
            .setNumAttempts(0)
            .setJobStatus(RECEIVED)
            .setServerJobId(UUID.randomUUID().toString())
            .setRequestInfo(
                RequestInfo.newBuilder()
                    .setJobRequestId(jobRequestId)
                    .setInputDataBlobPrefix("foobar.avro")
                    .setInputDataBucketName("foo-bar")
                    .setOutputDataBlobPrefix("foobar.avro")
                    .setOutputDataBucketName("foo-bar")
                    .setPostbackUrl("http://foo.com/bar")
                    .putAllJobParameters(
                        ImmutableMap.of(
                            "attribution_report_to",
                            "bar.com",
                            "output_domain_blob_prefix",
                            "foobar.avro",
                            "output_domain_bucket_name",
                            "foo-bar"))
                    .build())
            .build();
  }

  /** Test that getting a non-existent metadata item returns an empty optional */
  @Test
  public void getJobMetadata_getNonExistentReturnsEmpty() throws Exception {
    JobKey nonexistentJobKey =
        JobKey.newBuilder().setJobRequestId(UUID.randomUUID().toString()).build();

    Optional<JobMetadata> jobMetadata =
        spannerMetadataDb.getJobMetadata(nonexistentJobKey.getJobRequestId());

    assertThat(jobMetadata).isEmpty();
  }

  /** Test that a metadata item can be inserted and read back */
  @Test
  public void insertThenGet() throws Exception {
    spannerMetadataDb.insertJobMetadata(jobMetadata);
    Optional<JobMetadata> lookedUpJobMetadata =
        spannerMetadataDb.getJobMetadata(jobKey.getJobRequestId());

    assertThat(lookedUpJobMetadata).isPresent();
    assertJobMetadataEqualsIgnoreRecordVersion(
        lookedUpJobMetadata.get(),
        jobMetadata.toBuilder()
            .setJobKey(JobKey.newBuilder().setJobRequestId(jobKey.getJobRequestId()).build())
            .build());
  }

  /** Test that an exception is thrown when a duplicate metadata item is inserted */
  @Test
  public void insertJobMetadata_insertNonUniqueThrows() throws Exception {
    spannerMetadataDb.insertJobMetadata(jobMetadata);

    assertThrows(
        JobKeyExistsException.class, () -> spannerMetadataDb.insertJobMetadata(jobMetadata));
  }

  /** Test updating a metadata item */
  @Test
  public void updateJobMetadata_updatesItem() throws Exception {
    ResultInfo resultInfo =
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

    spannerMetadataDb.insertJobMetadata(jobMetadata);
    Optional<JobMetadata> lookedUpJobMetadata =
        spannerMetadataDb.getJobMetadata(jobKey.getJobRequestId());
    JobMetadata modifiedJobMetadata =
        lookedUpJobMetadata.get().toBuilder()
            .setJobStatus(FINISHED)
            .setResultInfo(resultInfo)
            .build();
    spannerMetadataDb.updateJobMetadata(modifiedJobMetadata);
    Optional<JobMetadata> secondLookedUpJobMetadata =
        spannerMetadataDb.getJobMetadata(jobKey.getJobRequestId());
    JobMetadata secondMetadata =
        secondLookedUpJobMetadata.get().toBuilder()
            .setRequestUpdatedAt(lookedUpJobMetadata.get().getRequestUpdatedAt())
            .build();

    assertThat(lookedUpJobMetadata).isPresent();
    assertJobMetadataEqualsIgnoreRecordVersion(secondMetadata, modifiedJobMetadata);
  }

  /** Test stale updating a metadata item does nothing */
  @Test
  public void updateJobMetadata_staleUpdateDropped() throws Exception {
    spannerMetadataDb.insertJobMetadata(jobMetadata);
    Optional<JobMetadata> lookedUpJobMetadata =
        spannerMetadataDb.getJobMetadata(jobKey.getJobRequestId());
    JobMetadata modifiedJobMetadata =
        lookedUpJobMetadata.get().toBuilder()
            .setRequestUpdatedAt(ProtoUtil.toProtoTimestamp(Instant.parse("2019-01-01T00:00:00Z")))
            .setJobStatus(FINISHED)
            .build();
    assertThrows(
        JobMetadataDbException.class,
        () -> spannerMetadataDb.updateJobMetadata(modifiedJobMetadata));
    Optional<JobMetadata> secondLookedUpJobMetadata =
        spannerMetadataDb.getJobMetadata(jobKey.getJobRequestId());

    assertThat(lookedUpJobMetadata).isPresent();
    assertJobMetadataEqualsIgnoreRecordVersion(
        secondLookedUpJobMetadata.get(), lookedUpJobMetadata.get());
  }

  /** Test updating a non-existent metadata item throws an exception */
  @Test
  public void updateJobMetadata_updateNonexistentThrows() throws Exception {
    assertThrows(
        JobMetadataDbException.class, () -> spannerMetadataDb.updateJobMetadata(jobMetadata));
  }

  /** Test that a metadata item with optional fields can be inserted and read back */
  @Test
  public void testInsertAndGetWithRequestProcessedAt() throws Exception {
    JobMetadata modifiedJobMetadata =
        jobMetadata.toBuilder()
            .setJobStatus(FINISHED)
            .setRequestProcessingStartedAt(
                ProtoUtil.toProtoTimestamp(Instant.parse("2021-01-01T00:00:00Z")))
            .build();

    spannerMetadataDb.insertJobMetadata(modifiedJobMetadata);
    Optional<JobMetadata> lookedUpJobMetadata =
        spannerMetadataDb.getJobMetadata(jobKey.getJobRequestId());

    assertThat(lookedUpJobMetadata).isPresent();
    assertJobMetadataEqualsIgnoreRecordVersion(lookedUpJobMetadata.get(), modifiedJobMetadata);
  }

  private static void assertJobMetadataEqualsIgnoreRecordVersion(
      JobMetadata actual, JobMetadata expected) {
    JobMetadata expectedNoVersionNumber = expected.toBuilder().clearRecordVersion().build();
    JobMetadata actualNoVerisonNumber = actual.toBuilder().clearRecordVersion().build();

    assertThat(actualNoVerisonNumber).isEqualTo(expectedNoVersionNumber);
  }
}
