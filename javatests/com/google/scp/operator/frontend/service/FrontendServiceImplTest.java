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

package com.google.scp.operator.frontend.service;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.operator.frontend.service.model.Constants.JOB_PARAM_ATTRIBUTION_REPORT_TO;
import static com.google.scp.operator.frontend.tasks.ErrorMessages.DB_ERROR_MESSAGE;
import static com.google.scp.operator.frontend.tasks.ErrorMessages.DUPLICATE_JOB_MESSAGE;
import static com.google.scp.operator.frontend.tasks.ErrorMessages.JOB_NOT_FOUND_MESSAGE;
import static com.google.scp.shared.api.exception.testing.ServiceExceptionAssertions.assertThatServiceExceptionMatches;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.protobuf.Timestamp;
import com.google.scp.operator.frontend.injection.modules.testing.FakeFrontendModule;
import com.google.scp.operator.frontend.tasks.ErrorReasons;
import com.google.scp.operator.frontend.testing.FakeRequestInfoValidator;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestProto.CreateJobRequest;
import com.google.scp.operator.protos.frontend.api.v1.GetJobResponseProto.GetJobResponse;
import com.google.scp.operator.protos.frontend.api.v1.JobStatusProto;
import com.google.scp.operator.protos.shared.backend.JobKeyProto.JobKey;
import com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.testing.FakeMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.proto.ProtoUtil;
import java.text.ParseException;
import java.time.Clock;
import java.time.Instant;
import java.util.Optional;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class FrontendServiceImplTest {

  final String REQUEST_ID = "foo";

  @Rule public Acai acai = new Acai(TestEnv.class);
  // Under test
  @Inject FrontendServiceImpl frontendService;
  @Inject FakeMetadataDb fakeMetadataDb;
  @Inject FakeRequestInfoValidator fakeRequestInfoValidator;
  @Inject Clock clock;

  CreateJobRequest createJobRequest;
  String attributionReportTo;
  JobMetadata jobMetadata;
  JobMetadata receivedJobMetadata;
  JobMetadata processedJobMetadata;
  JobMetadata finishedJobMetadata;
  JobKey jobKey;
  Instant clockTime;
  Timestamp clockTimestamp;

  @Before
  public void setUp() throws ParseException {
    clockTime = clock.instant();
    clockTimestamp = ProtoUtil.toProtoTimestamp(clockTime);
    createJobRequest = ServiceJobGenerator.createFakeCreateJobRequest(REQUEST_ID);
    attributionReportTo =
        createJobRequest.getJobParametersMap().get(JOB_PARAM_ATTRIBUTION_REPORT_TO);
    jobMetadata = JobGenerator.createFakeJobMetadata(REQUEST_ID);
    receivedJobMetadata =
        jobMetadata.toBuilder()
            .setRequestReceivedAt(ProtoUtil.toProtoTimestamp(clockTime))
            .setRequestUpdatedAt(ProtoUtil.toProtoTimestamp(clockTime))
            .setJobStatus(JobStatus.RECEIVED)
            .clearRecordVersion()
            .clearResultInfo()
            .clearCreateJobRequest()
            .setRequestInfo(jobMetadata.getRequestInfo())
            .build();
    processedJobMetadata =
        receivedJobMetadata.toBuilder()
            .setJobStatus(JobStatus.IN_PROGRESS)
            .setRequestProcessingStartedAt(ProtoUtil.toProtoTimestamp(clockTime))
            .build();
    finishedJobMetadata =
        jobMetadata.toBuilder()
            .setJobStatus(JobStatus.FINISHED)
            .setRequestReceivedAt(ProtoUtil.toProtoTimestamp(clockTime))
            .setRequestUpdatedAt(ProtoUtil.toProtoTimestamp(clockTime))
            .setRequestProcessingStartedAt(ProtoUtil.toProtoTimestamp(clockTime))
            .clearRecordVersion()
            .setRequestInfo(jobMetadata.getRequestInfo())
            .build();
    jobKey = jobMetadata.getJobKey();
    fakeMetadataDb.reset();
  }

  /** Test for scenario to insert a job with no failures */
  @Test
  public void testCreateJob_simple() throws Exception {

    frontendService.createJob(createJobRequest);

    // No assertion on the CreateJobResponse that is returned since it has no fields to check
    assertThat(fakeMetadataDb.getLastJobMetadataInserted().toBuilder().setServerJobId("").build())
        .isEqualTo(receivedJobMetadata);
  }

  /** Test for scenario to insert a job with a validation failure */
  @Test
  public void testCreateJob_validationFailure() {
    String validationErrorMessage = "Oh no a validation failure";
    fakeRequestInfoValidator.setValidateReturnValue(Optional.of(validationErrorMessage));

    ServiceException serviceException =
        assertThrows(ServiceException.class, () -> frontendService.createJob(createJobRequest));

    ServiceException expectedServiceException =
        new ServiceException(
            Code.INVALID_ARGUMENT,
            ErrorReasons.VALIDATION_FAILED.toString(),
            validationErrorMessage);
    assertThatServiceExceptionMatches(serviceException, expectedServiceException);
    assertThat(fakeMetadataDb.getLastJobMetadataInserted()).isNull();
  }

  /** Test for scenario to insert a job where the JobKey is taken */
  @Test
  public void testCreateJob_duplicateJobKey() {
    fakeMetadataDb.setShouldThrowJobKeyExistsException(true);

    ServiceException serviceException =
        assertThrows(ServiceException.class, () -> frontendService.createJob(createJobRequest));

    ServiceException expectedServiceException =
        new ServiceException(
            Code.ALREADY_EXISTS,
            ErrorReasons.DUPLICATE_JOB_KEY.toString(),
            String.format(DUPLICATE_JOB_MESSAGE, createJobRequest.getJobRequestId()));
    assertThatServiceExceptionMatches(serviceException, expectedServiceException);
    assertThat(fakeMetadataDb.getLastJobMetadataInserted()).isNull();
  }

  /** Test for scenario to insert a job with some internal error */
  @Test
  public void testCreateJob_exception() {
    fakeMetadataDb.setShouldThrowJobMetadataDbException(true);

    ServiceException serviceException =
        assertThrows(ServiceException.class, () -> frontendService.createJob(createJobRequest));

    ServiceException expectedServiceException =
        new ServiceException(Code.INTERNAL, ErrorReasons.SERVER_ERROR.toString(), DB_ERROR_MESSAGE);
    assertThatServiceExceptionMatches(serviceException, expectedServiceException);
  }

  /** Test for scenario to get a job with received status that exists with no failures */
  @Test
  public void testGetJob_received() throws Exception {
    fakeMetadataDb.setJobMetadataToReturn(Optional.of(receivedJobMetadata));

    GetJobResponse getJobResponse = frontendService.getJob(REQUEST_ID);

    assertThat(getJobResponse)
        .isEqualTo(
            ServiceJobGenerator.createFakeJobResponse(REQUEST_ID).toBuilder()
                .setRequestReceivedAt(clockTimestamp)
                .setRequestUpdatedAt(clockTimestamp)
                .setJobStatus(JobStatusProto.JobStatus.RECEIVED)
                .clearResultInfo()
                .build());
    assertThat(fakeMetadataDb.getLastJobKeyStringLookedUp()).isEqualTo(jobKey.getJobRequestId());
  }

  /** Test for scenario to get a job with the processed status that exists with no failures */
  @Test
  public void testGetJob_processed() throws Exception {
    fakeMetadataDb.setJobMetadataToReturn(Optional.of(processedJobMetadata));

    GetJobResponse getJobResponse = frontendService.getJob(REQUEST_ID);

    assertThat(getJobResponse)
        .isEqualTo(
            ServiceJobGenerator.createFakeJobResponse(REQUEST_ID).toBuilder()
                .setRequestReceivedAt(clockTimestamp)
                .setRequestUpdatedAt(clockTimestamp)
                .setJobStatus(JobStatusProto.JobStatus.IN_PROGRESS)
                .setRequestProcessingStartedAt(clockTimestamp)
                .clearResultInfo()
                .build());
    assertThat(fakeMetadataDb.getLastJobKeyStringLookedUp()).isEqualTo(jobKey.getJobRequestId());
  }

  /** Test for scenario to get a job with the finished status that exists with no failures */
  @Test
  public void testGetJob_finished() throws Exception {
    fakeMetadataDb.setJobMetadataToReturn(Optional.of(finishedJobMetadata));

    GetJobResponse getJobResponse = frontendService.getJob(REQUEST_ID);

    assertThat(getJobResponse)
        .isEqualTo(
            ServiceJobGenerator.createFakeJobResponse(REQUEST_ID).toBuilder()
                .setRequestReceivedAt(clockTimestamp)
                .setRequestUpdatedAt(clockTimestamp)
                .setJobStatus(JobStatusProto.JobStatus.FINISHED)
                .setRequestProcessingStartedAt(clockTimestamp)
                .build());
    assertThat(fakeMetadataDb.getLastJobKeyStringLookedUp()).isEqualTo(jobKey.getJobRequestId());
  }

  /** Test for scenario to get a job that does not exist with no failures */
  @Test
  public void testGetJob_doesNotExist() {
    fakeMetadataDb.setJobMetadataToReturn(Optional.empty());

    ServiceException serviceException =
        assertThrows(ServiceException.class, () -> frontendService.getJob(REQUEST_ID));

    ServiceException expectedServiceException =
        new ServiceException(
            Code.NOT_FOUND,
            ErrorReasons.JOB_NOT_FOUND.toString(),
            String.format(JOB_NOT_FOUND_MESSAGE, REQUEST_ID));
    assertThatServiceExceptionMatches(serviceException, expectedServiceException);
    assertThat(fakeMetadataDb.getLastJobKeyStringLookedUp()).isEqualTo(jobKey.getJobRequestId());
  }

  /** Test for scenario to get a job with some internal error happening */
  @Test
  public void testGetJob_exception() {
    fakeMetadataDb.setShouldThrowJobMetadataDbException(true);

    ServiceException serviceException =
        assertThrows(ServiceException.class, () -> frontendService.getJob(REQUEST_ID));

    ServiceException expectedServiceException =
        new ServiceException(Code.INTERNAL, ErrorReasons.SERVER_ERROR.toString(), DB_ERROR_MESSAGE);
    assertThatServiceExceptionMatches(serviceException, expectedServiceException);
  }

  static class TestEnv extends AbstractModule {

    @Override
    public void configure() {
      install(new FakeFrontendModule());
    }
  }
}
