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

package com.google.scp.operator.shared.testing;

import static com.google.common.truth.Truth.assertThat;

import com.amazonaws.services.lambda.runtime.events.DynamodbEvent.DynamodbStreamRecord;
import com.amazonaws.services.lambda.runtime.events.transformers.v2.dynamodb.DynamodbRecordTransformer;
import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.frontend.service.ServiceJobGenerator;
import com.google.scp.operator.frontend.service.model.Constants;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestProto.CreateJobRequest;
import com.google.scp.operator.protos.frontend.api.v1.GetJobResponseProto.GetJobResponse;
import com.google.scp.operator.protos.frontend.api.v1.JobStatusProto.JobStatus;
import com.google.scp.operator.protos.frontend.api.v1.ResultInfoProto.ResultInfo;
import com.google.scp.operator.protos.shared.backend.JobErrorCategoryProto.JobErrorCategory;
import com.google.scp.operator.protos.shared.backend.JobKeyProto.JobKey;
import com.google.scp.operator.protos.shared.backend.RequestInfoProto.RequestInfo;
import com.google.scp.operator.protos.shared.backend.ReturnCodeProto.ReturnCode;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.aws.model.converter.AttributeValueMapToJobMetadataConverter;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.shared.proto.ProtoUtil;
import java.text.ParseException;
import java.time.Instant;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.services.dynamodb.model.Record;

@RunWith(JUnit4.class)
public class JobGeneratorTest {

  private static final String JOB_PARAM_ATTRIBUTION_REPORT_TO = "attribution_report_to";
  private static final String JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT = "debug_privacy_budget_limit";

  @Rule public final Acai acai = new Acai(TestEnv.class);

  @Inject AttributeValueMapToJobMetadataConverter attributeValueMapConverter;

  @Test
  public void testCreateFakeJobMetadata() {
    String jobRequestId = "foo-bar";
    String dataHandle = "dataHandle";
    String dataHandleBucket = "bucket";
    String postbackUrl = "http://postback.com";
    ImmutableList<String> privacyBudgetKeys =
        ImmutableList.of("privacyBudgetKey1", "privacyBudgetKey2");
    String attributionReportTo = "foo.com";
    Integer debugPrivacyBudgetLimit = 5;
    Instant requestReceivedAt = Instant.parse("2019-10-01T08:25:24.00Z");
    Instant requestUpdatedAt = Instant.parse("2019-10-01T08:29:24.00Z");
    com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus jobStatus =
        com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus.IN_PROGRESS;
    ImmutableList<Instant> reportingWindows =
        ImmutableList.of(
            Instant.parse("2019-09-01T02:00:00.00Z"), Instant.parse("2019-09-01T02:30:00.00Z"));
    int recordVersion = 2;
    com.google.scp.operator.protos.shared.backend.ResultInfoProto.ResultInfo resultInfo =
        com.google.scp.operator.protos.shared.backend.ResultInfoProto.ResultInfo.newBuilder()
            .setErrorSummary(
                com.google.scp.operator.protos.shared.backend.ErrorSummaryProto.ErrorSummary
                    .newBuilder()
                    .addAllErrorCounts(
                        ImmutableList.of(
                            com.google.scp.operator.protos.shared.backend.ErrorCountProto.ErrorCount
                                .newBuilder()
                                .setCategory(JobErrorCategory.DECRYPTION_ERROR.name())
                                .setCount(5L)
                                .setDescription("Decryption error.")
                                .build(),
                            com.google.scp.operator.protos.shared.backend.ErrorCountProto.ErrorCount
                                .newBuilder()
                                .setCategory(JobErrorCategory.GENERAL_ERROR.name())
                                .setCount(12L)
                                .setDescription("General error.")
                                .build(),
                            com.google.scp.operator.protos.shared.backend.ErrorCountProto.ErrorCount
                                .newBuilder()
                                .setCategory(JobErrorCategory.NUM_REPORTS_WITH_ERRORS.name())
                                .setCount(17L)
                                .setDescription("Total number of reports with error.")
                                .build()))
                    .build())
            .setFinishedAt(ProtoUtil.toProtoTimestamp(Instant.parse("2019-10-01T13:25:24.00Z")))
            .setReturnCode(ReturnCode.SUCCESS.name())
            .setReturnMessage("Aggregation job successfully processed")
            .build();
    JobKey jobKey = JobKey.newBuilder().setJobRequestId(jobRequestId).build();
    com.google.scp.operator.protos.shared.backend.CreateJobRequestProto.CreateJobRequest
        createJobRequest =
            com.google.scp.operator.protos.shared.backend.CreateJobRequestProto.CreateJobRequest
                .newBuilder()
                .setJobRequestId(jobRequestId)
                .setInputDataBlobPrefix(dataHandle)
                .setInputDataBucketName(dataHandleBucket)
                .setOutputDataBlobPrefix(dataHandle)
                .setOutputDataBucketName(dataHandleBucket)
                .setPostbackUrl(postbackUrl)
                .setAttributionReportTo(attributionReportTo)
                .setDebugPrivacyBudgetLimit(debugPrivacyBudgetLimit)
                .putAllJobParameters(
                    ImmutableMap.of(
                        JOB_PARAM_ATTRIBUTION_REPORT_TO,
                        attributionReportTo,
                        JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT,
                        debugPrivacyBudgetLimit.toString()))
                .build();
    RequestInfo requestInfo =
        com.google.scp.operator.protos.shared.backend.RequestInfoProto.RequestInfo.newBuilder()
            .setJobRequestId(jobRequestId)
            .setInputDataBlobPrefix(dataHandle)
            .setInputDataBucketName(dataHandleBucket)
            .setOutputDataBlobPrefix(dataHandle)
            .setOutputDataBucketName(dataHandleBucket)
            .setPostbackUrl(postbackUrl)
            .putAllJobParameters(
                ImmutableMap.of(
                    JOB_PARAM_ATTRIBUTION_REPORT_TO,
                    attributionReportTo,
                    JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT,
                    debugPrivacyBudgetLimit.toString()))
            .build();

    JobMetadata expectedJobMetadata =
        JobMetadata.newBuilder()
            .setCreateJobRequest(createJobRequest)
            .setJobStatus(jobStatus)
            .setRequestReceivedAt(ProtoUtil.toProtoTimestamp(requestReceivedAt))
            .setRequestUpdatedAt(ProtoUtil.toProtoTimestamp(requestUpdatedAt))
            .setNumAttempts(0)
            .setJobKey(jobKey)
            .setRecordVersion(recordVersion)
            .setRequestInfo(requestInfo)
            .setResultInfo(resultInfo)
            .build();

    JobMetadata actualJobMetadata = JobGenerator.createFakeJobMetadata("foo-bar");

    assertThat(actualJobMetadata).isEqualTo(expectedJobMetadata);
  }

  @Test
  public void testDynamoStreamRecordEqualsJobMetadata() {
    String requestId = "123";
    JobMetadata expectedNewImageJobMetadata = JobGenerator.createFakeJobMetadata(requestId);
    JobMetadata expectedOldImageJobMetadata =
        expectedNewImageJobMetadata.toBuilder()
            .setJobStatus(
                com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus.RECEIVED)
            .setRequestUpdatedAt(
                ProtoUtil.toProtoTimestamp(Instant.parse("2019-10-01T08:25:24.00Z")))
            .setRecordVersion(1)
            .build();

    // Get the stream record from the JobGenerator and convert its new and old images to JobMetadata
    // objects to compare against.
    DynamodbStreamRecord streamRecord = JobGenerator.createFakeDynamodbStreamRecord(requestId);
    Record sdkV2Record = DynamodbRecordTransformer.toRecordV2(streamRecord);
    JobMetadata actualOldImageJobMetadata =
        attributeValueMapConverter.convert(sdkV2Record.dynamodb().oldImage());
    JobMetadata actualNewImageJobMetadata =
        attributeValueMapConverter.convert(sdkV2Record.dynamodb().newImage());

    assertThat(actualOldImageJobMetadata).isEqualTo(expectedOldImageJobMetadata);
    assertThat(actualNewImageJobMetadata).isEqualTo(expectedNewImageJobMetadata);
  }

  @Test
  public void testGetJobResponseEqualsJobMetadata() throws ParseException {
    String requestId = "123";
    JobMetadata fakeJobMetadata = JobGenerator.createFakeJobMetadata(requestId);
    GetJobResponse fakeJobResponse = ServiceJobGenerator.createFakeJobResponse(requestId);
    ResultInfo fakeResultInfo = ServiceJobGenerator.createFakeResultInfo();

    // Assert direct properties of JobMetadata are not null
    assertThat(fakeJobMetadata.hasRequestReceivedAt()).isTrue();
    assertThat(fakeJobResponse.hasRequestReceivedAt()).isTrue();

    // Assert that JobKey properties are not null
    assertThat(fakeJobMetadata.hasJobKey()).isTrue();
    assertThat(fakeJobMetadata.getJobKey().getJobRequestId()).isNotEmpty();

    // Assert that properties are not null
    assertThat(fakeJobMetadata.getCreateJobRequest().getJobRequestId()).isNotEmpty();
    assertThat(fakeJobMetadata.getRequestInfo().getJobRequestId()).isNotEmpty();
    assertThat(fakeJobResponse.getJobRequestId()).isNotEmpty();

    assertThat(fakeJobMetadata.getCreateJobRequest().getAttributionReportTo()).isNotEmpty();
    assertThat(fakeJobResponse.getJobParametersMap().get(Constants.JOB_PARAM_ATTRIBUTION_REPORT_TO))
        .isNotNull();

    assertThat(fakeJobMetadata.getCreateJobRequest().getInputDataBlobPrefix()).isNotEmpty();
    assertThat(fakeJobMetadata.getRequestInfo().getInputDataBlobPrefix()).isNotEmpty();
    assertThat(fakeJobResponse.getInputDataBlobPrefix()).isNotEmpty();

    assertThat(fakeJobMetadata.getCreateJobRequest().getInputDataBucketName()).isNotEmpty();
    assertThat(fakeJobMetadata.getRequestInfo().getInputDataBucketName()).isNotEmpty();
    assertThat(fakeJobResponse.getInputDataBucketName()).isNotEmpty();

    assertThat(fakeJobMetadata.getCreateJobRequest().getPostbackUrl()).isNotEmpty();
    assertThat(fakeJobMetadata.getRequestInfo().getPostbackUrl()).isNotEmpty();
    assertThat(fakeJobResponse.getPostbackUrl()).isNotEmpty();

    // Assert that all fields of GetJobResponse match fields on the JobMetadata
    assertThat(fakeJobResponse)
        .isEqualTo(
            GetJobResponse.newBuilder()
                .setJobRequestId(fakeJobMetadata.getRequestInfo().getJobRequestId())
                .setJobStatus(JobStatus.valueOf(fakeJobMetadata.getJobStatus().name()))
                .setRequestReceivedAt(fakeJobMetadata.getRequestReceivedAt())
                .setRequestUpdatedAt(fakeJobMetadata.getRequestUpdatedAt())
                .setInputDataBlobPrefix(fakeJobMetadata.getRequestInfo().getInputDataBlobPrefix())
                .setInputDataBucketName(fakeJobMetadata.getRequestInfo().getInputDataBucketName())
                .setOutputDataBlobPrefix(fakeJobMetadata.getRequestInfo().getOutputDataBlobPrefix())
                .setOutputDataBucketName(fakeJobMetadata.getRequestInfo().getOutputDataBucketName())
                .putAllJobParameters(fakeJobMetadata.getRequestInfo().getJobParameters())
                .setPostbackUrl(fakeJobMetadata.getRequestInfo().getPostbackUrl())
                .setResultInfo(fakeResultInfo)
                .build());
  }

  @Test
  public void testCreateJobRequestEqualsJobMetadata() {
    String requestId = "123";
    JobMetadata fakeJobMetadata = JobGenerator.createFakeJobMetadata(requestId);

    CreateJobRequest fakeCreateJobRequest =
        ServiceJobGenerator.createFakeCreateJobRequest(requestId);

    // Assert direct properties of JobMetadata are not null
    assertThat(fakeJobMetadata.hasRequestReceivedAt()).isTrue();

    // Assert that JobKey properties are not null
    assertThat(fakeJobMetadata.hasJobKey()).isTrue();
    assertThat(fakeJobMetadata.getJobKey().getJobRequestId()).isNotEmpty();

    // Assert that properties are not null
    assertThat(fakeJobMetadata.getCreateJobRequest().getJobRequestId()).isNotEmpty();

    assertThat(fakeJobMetadata.getCreateJobRequest().getAttributionReportTo()).isNotEmpty();
    assertThat(
            fakeCreateJobRequest
                .getJobParametersMap()
                .get(Constants.JOB_PARAM_ATTRIBUTION_REPORT_TO))
        .isNotNull();

    assertThat(fakeJobMetadata.getCreateJobRequest().getJobRequestId()).isNotEmpty();
    assertThat(fakeCreateJobRequest.getJobRequestId()).isNotEmpty();

    assertThat(fakeJobMetadata.getCreateJobRequest().getInputDataBlobPrefix()).isNotEmpty();
    assertThat(fakeCreateJobRequest.getInputDataBlobPrefix()).isNotEmpty();

    assertThat(fakeJobMetadata.getCreateJobRequest().getInputDataBucketName()).isNotEmpty();
    assertThat(fakeCreateJobRequest.getInputDataBucketName()).isNotEmpty();

    assertThat(fakeJobMetadata.getCreateJobRequest().getPostbackUrl()).isNotEmpty();
    assertThat(fakeCreateJobRequest.getPostbackUrl()).isNotEmpty();

    // Assert that all fields of CreateJobRequest match fields on the JobMetadata
    assertThat(fakeCreateJobRequest)
        .isEqualTo(
            CreateJobRequest.newBuilder()
                .setJobRequestId(fakeJobMetadata.getCreateJobRequest().getJobRequestId())
                .setInputDataBlobPrefix(
                    fakeJobMetadata.getCreateJobRequest().getInputDataBlobPrefix())
                .setInputDataBucketName(
                    fakeJobMetadata.getCreateJobRequest().getInputDataBucketName())
                .setOutputDataBlobPrefix(
                    fakeJobMetadata.getCreateJobRequest().getOutputDataBlobPrefix())
                .setOutputDataBucketName(
                    fakeJobMetadata.getCreateJobRequest().getOutputDataBucketName())
                .setPostbackUrl(fakeJobMetadata.getCreateJobRequest().getPostbackUrl())
                .putAllJobParameters(fakeJobMetadata.getCreateJobRequest().getJobParameters())
                .build());
  }

  @Test
  public void testCreateJobRequestSharedEqualsJobMetadata() {
    String requestId = "123";
    JobMetadata fakeJobMetadata = JobGenerator.createFakeJobMetadata(requestId);

    com.google.scp.operator.protos.shared.backend.CreateJobRequestProto.CreateJobRequest
        fakeCreateJobRequest = JobGenerator.createFakeCreateJobRequestShared(requestId);

    // Assert direct properties of JobMetadata are not null
    assertThat(fakeJobMetadata.hasRequestReceivedAt()).isTrue();

    // Assert that JobKey properties are not null
    assertThat(fakeJobMetadata.hasJobKey()).isTrue();
    assertThat(fakeJobMetadata.getJobKey().getJobRequestId()).isNotEmpty();

    // Assert that properties are not null
    assertThat(fakeJobMetadata.getCreateJobRequest().getJobRequestId()).isNotEmpty();

    assertThat(fakeJobMetadata.getCreateJobRequest().getAttributionReportTo()).isNotEmpty();
    assertThat(fakeCreateJobRequest.getAttributionReportTo()).isNotEmpty();

    assertThat(fakeJobMetadata.getCreateJobRequest().getJobRequestId()).isNotEmpty();
    assertThat(fakeCreateJobRequest.getJobRequestId()).isNotEmpty();

    assertThat(fakeJobMetadata.getCreateJobRequest().getInputDataBlobPrefix()).isNotEmpty();
    assertThat(fakeCreateJobRequest.getInputDataBlobPrefix()).isNotEmpty();

    assertThat(fakeJobMetadata.getCreateJobRequest().getInputDataBucketName()).isNotEmpty();
    assertThat(fakeCreateJobRequest.getInputDataBucketName()).isNotEmpty();

    assertThat(fakeJobMetadata.getCreateJobRequest().getPostbackUrl()).isNotEmpty();
    assertThat(fakeCreateJobRequest.getPostbackUrl()).isNotEmpty();

    // Assert that all fields of CreateJobRequest match fields on the JobMetadata
    assertThat(fakeCreateJobRequest)
        .isEqualTo(
            com.google.scp.operator.protos.shared.backend.CreateJobRequestProto.CreateJobRequest
                .newBuilder()
                .setJobRequestId(fakeJobMetadata.getCreateJobRequest().getJobRequestId())
                .setAttributionReportTo(
                    fakeJobMetadata.getCreateJobRequest().getAttributionReportTo())
                .setInputDataBlobPrefix(
                    fakeJobMetadata.getCreateJobRequest().getInputDataBlobPrefix())
                .setInputDataBucketName(
                    fakeJobMetadata.getCreateJobRequest().getInputDataBucketName())
                .setOutputDataBlobPrefix(
                    fakeJobMetadata.getCreateJobRequest().getOutputDataBlobPrefix())
                .setOutputDataBucketName(
                    fakeJobMetadata.getCreateJobRequest().getOutputDataBucketName())
                .setOutputDomainBlobPrefix(
                    fakeJobMetadata.getCreateJobRequest().getOutputDomainBlobPrefix())
                .setOutputDomainBucketName(
                    fakeJobMetadata.getCreateJobRequest().getOutputDomainBucketName())
                .setPostbackUrl(fakeJobMetadata.getCreateJobRequest().getPostbackUrl())
                .setDebugPrivacyBudgetLimit(
                    fakeJobMetadata.getCreateJobRequest().getDebugPrivacyBudgetLimit())
                .putAllJobParameters(fakeJobMetadata.getCreateJobRequest().getJobParameters())
                .build());
  }

  @Test
  public void testRequestInfoEqualsJobMetadata() {
    String requestId = "123";
    JobMetadata fakeJobMetadata = JobGenerator.createFakeJobMetadata(requestId);

    RequestInfo fakeRequestInfo = JobGenerator.createFakeRequestInfo(requestId);

    // Assert direct properties of JobMetadata are not null
    assertThat(fakeJobMetadata.hasRequestReceivedAt()).isTrue();

    // Assert that JobKey properties are not null
    assertThat(fakeJobMetadata.hasJobKey()).isTrue();
    assertThat(fakeJobMetadata.getJobKey().getJobRequestId()).isNotEmpty();

    // Assert that properties are not null
    assertThat(fakeJobMetadata.getRequestInfo().getJobRequestId()).isNotEmpty();
    assertThat(fakeRequestInfo.getJobRequestId()).isNotEmpty();

    assertThat(fakeJobMetadata.getRequestInfo().getInputDataBlobPrefix()).isNotEmpty();
    assertThat(fakeRequestInfo.getInputDataBlobPrefix()).isNotEmpty();

    assertThat(fakeJobMetadata.getRequestInfo().getInputDataBucketName()).isNotEmpty();
    assertThat(fakeRequestInfo.getInputDataBucketName()).isNotEmpty();

    assertThat(fakeJobMetadata.getRequestInfo().getPostbackUrl()).isNotEmpty();
    assertThat(fakeRequestInfo.getPostbackUrl()).isNotEmpty();

    // Assert that all fields of CreateJobRequest match fields on the JobMetadata
    assertThat(fakeRequestInfo)
        .isEqualTo(
            RequestInfo.newBuilder()
                .setJobRequestId(fakeJobMetadata.getRequestInfo().getJobRequestId())
                .setInputDataBlobPrefix(fakeJobMetadata.getRequestInfo().getInputDataBlobPrefix())
                .setInputDataBucketName(fakeJobMetadata.getRequestInfo().getInputDataBucketName())
                .setOutputDataBlobPrefix(fakeJobMetadata.getRequestInfo().getOutputDataBlobPrefix())
                .setOutputDataBucketName(fakeJobMetadata.getRequestInfo().getOutputDataBucketName())
                .setPostbackUrl(fakeJobMetadata.getRequestInfo().getPostbackUrl())
                .putAllJobParameters(fakeJobMetadata.getRequestInfo().getJobParameters())
                .build());
  }

  public static class TestEnv extends AbstractModule {}
}
