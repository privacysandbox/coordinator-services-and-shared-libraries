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

package com.google.scp.operator.frontend.service.aws.changehandler;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.operator.frontend.service.aws.changehandler.MarkJobFailedToEnqueueHandler.RETURN_MESSAGE;
import static com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus.FINISHED;
import static com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus.IN_PROGRESS;
import static com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus.RECEIVED;
import static com.google.scp.operator.protos.shared.backend.ReturnCodeProto.ReturnCode.INTERNAL_ERROR;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.acai.TestScoped;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.frontend.service.aws.changehandler.JobMetadataChangeHandler.ChangeHandlerException;
import com.google.scp.operator.protos.shared.backend.ErrorSummaryProto.ErrorSummary;
import com.google.scp.operator.protos.shared.backend.ResultInfoProto.ResultInfo;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobMetadataDbException;
import com.google.scp.operator.shared.dao.metadatadb.testing.FakeMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.shared.proto.ProtoUtil;
import java.time.Clock;
import java.time.Instant;
import java.time.ZoneId;
import java.util.Optional;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class MarkJobFailedToEnqueueHandlerTest {

  private static final Instant TIME = Instant.parse("2021-01-01T00:00:00Z");

  @Rule public Acai acai = new Acai(TestEnv.class);

  @Inject MarkJobFailedToEnqueueHandler markJobFailedToEnqueueHandler;

  @Inject FakeMetadataDb fakeMetadataDb;

  JobMetadata receivedJobMetadata;
  JobMetadata inProgressJobMetadata;

  @Before
  public void setUp() {
    // Use the JobGenerator to create test objects, then set the status appropriately
    JobMetadata jobMetadata = JobGenerator.createFakeJobMetadata("foo");
    receivedJobMetadata = jobMetadata.toBuilder().setJobStatus(RECEIVED).clearResultInfo().build();
    inProgressJobMetadata =
        jobMetadata.toBuilder().setJobStatus(IN_PROGRESS).clearResultInfo().build();
  }

  @Test
  public void canHandle_returnsTrueForReceived() {
    // No setup

    boolean canHandle = markJobFailedToEnqueueHandler.canHandle(receivedJobMetadata);

    assertThat(canHandle).isTrue();
  }

  @Test
  public void canHandle_returnsFalseForInProgress() {
    // No setup

    boolean canHandle = markJobFailedToEnqueueHandler.canHandle(inProgressJobMetadata);

    assertThat(canHandle).isFalse();
  }

  /**
   * Test that the job is marked as failed when {@link MarkJobFailedToEnqueueHandler#handle} is
   * called.
   */
  @Test
  public void handle_noExceptionMarksCleanedUpInDb() {
    JobMetadata expectedUpdatedJobMetadata =
        receivedJobMetadata.toBuilder()
            .setJobStatus(FINISHED)
            .setResultInfo(
                ResultInfo.newBuilder()
                    .setReturnCode(INTERNAL_ERROR.name())
                    .setReturnMessage(RETURN_MESSAGE)
                    .setErrorSummary(ErrorSummary.getDefaultInstance())
                    .setFinishedAt(ProtoUtil.toProtoTimestamp(TIME))
                    .build())
            .build();

    markJobFailedToEnqueueHandler.handle(receivedJobMetadata);

    assertThat(fakeMetadataDb.getLastJobMetadataUpdated()).isEqualTo(expectedUpdatedJobMetadata);
  }

  /**
   * Test that if a JobMetadataConflictException is thrown then {@link
   * MarkJobFailedToEnqueueHandler#handle} will return without error and no change will be writen to
   * the DB.
   */
  @Test
  public void handle_dbConflictExceptionNoExceptionNoUpdate() {
    fakeMetadataDb.setShouldThrowJobMetadataConflictException(true);
    fakeMetadataDb.setJobMetadataToReturn(Optional.of(inProgressJobMetadata));

    markJobFailedToEnqueueHandler.handle(receivedJobMetadata);

    assertThat(fakeMetadataDb.getLastJobMetadataUpdated()).isNull();
  }

  /** Test that if JobMetadataDbException is thrown then a run exception is thrown */
  @Test
  public void handle_dbExceptionThrowsException() {
    fakeMetadataDb.setShouldThrowJobMetadataDbException(true);

    Exception exception =
        assertThrows(
            ChangeHandlerException.class,
            () -> markJobFailedToEnqueueHandler.handle(receivedJobMetadata));

    assertThat(exception).hasCauseThat().isInstanceOf(JobMetadataDbException.class);
  }

  private static final class TestEnv extends AbstractModule {

    @Override
    public void configure() {
      bind(FakeMetadataDb.class).in(TestScoped.class);
      bind(JobMetadataDb.class).to(FakeMetadataDb.class);
      bind(Clock.class).toInstance(Clock.fixed(TIME, ZoneId.systemDefault()));
    }
  }
}
