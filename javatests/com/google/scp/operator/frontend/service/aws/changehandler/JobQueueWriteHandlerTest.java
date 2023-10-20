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
import static com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus.IN_PROGRESS;
import static com.google.scp.operator.protos.shared.backend.JobStatusProto.JobStatus.RECEIVED;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.acai.TestScoped;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.frontend.service.aws.changehandler.JobMetadataChangeHandler.ChangeHandlerException;
import com.google.scp.operator.protos.shared.backend.JobKeyProto.JobKey;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue.JobQueueException;
import com.google.scp.operator.shared.dao.jobqueue.testing.FakeJobQueue;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

public class JobQueueWriteHandlerTest {

  @Rule public Acai acai = new Acai(TestEnv.class);

  // Under test
  @Inject JobQueueWriteHandler jobQueueWriteHandler;

  @Inject FakeJobQueue fakeJobQueue;

  JobMetadata receivedJobMetadata;
  JobMetadata inProgressJobMetadata;

  @Before
  public void setUp() {
    // Use the JobGenerator to create test objects, then set the status appropriately
    JobMetadata jobMetadata = JobGenerator.createFakeJobMetadata("foo");
    receivedJobMetadata = jobMetadata.toBuilder().setJobStatus(RECEIVED).build();
    inProgressJobMetadata = jobMetadata.toBuilder().setJobStatus(IN_PROGRESS).build();
  }

  @Test
  public void canHandle_returnsTrueForReceived() {
    // No setup

    boolean canHandle = jobQueueWriteHandler.canHandle(receivedJobMetadata);

    assertThat(canHandle).isTrue();
  }

  @Test
  public void canHandle_returnsFalseForInProgress() {
    // No setup

    boolean canHandle = jobQueueWriteHandler.canHandle(inProgressJobMetadata);

    assertThat(canHandle).isFalse();
  }

  @Test
  public void handlePutsJobOnQueue() {
    // No setup

    jobQueueWriteHandler.handle(receivedJobMetadata);
    JobKey lastJobKeySent = fakeJobQueue.getLastJobKeySent();

    assertThat(lastJobKeySent).isEqualTo(receivedJobMetadata.getJobKey());
  }

  @Test
  public void handleThrowsRunTimeExceptionWhenJobQueueThrowsException() {
    fakeJobQueue.setShouldThrowException(true);

    Exception exception =
        assertThrows(
            ChangeHandlerException.class, () -> jobQueueWriteHandler.handle(receivedJobMetadata));

    assertThat(exception).isInstanceOf(RuntimeException.class);
    assertThat(exception).hasCauseThat().isInstanceOf(JobQueueException.class);
  }

  private static final class TestEnv extends AbstractModule {

    @Override
    public void configure() {
      bind(FakeJobQueue.class).in(TestScoped.class);
      bind(JobQueue.class).to(FakeJobQueue.class);
    }
  }
}
