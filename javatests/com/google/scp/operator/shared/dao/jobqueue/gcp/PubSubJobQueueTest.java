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

package com.google.scp.operator.shared.dao.jobqueue.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.inject.Inject;
import com.google.protobuf.util.Durations;
import com.google.scp.operator.protos.shared.backend.JobKeyProto.JobKey;
import com.google.scp.operator.protos.shared.backend.jobqueue.JobQueueProto.JobQueueItem;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue.JobQueueException;
import com.google.scp.shared.proto.ProtoUtil;
import java.time.Duration;
import java.time.Instant;
import java.util.Optional;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Hermetic Test for the PubSubJobQueue. */
@RunWith(JUnit4.class)
public final class PubSubJobQueueTest {

  @Rule public Acai acai = new Acai(PubSubJobQueueTestModule.class);

  // Under test
  @Inject PubSubJobQueue pubSubJobQueue;

  JobKey jobKey;
  String serverJobId;

  @Before
  public void setUp() throws JobQueueException {
    jobKey = JobKey.newBuilder().setJobRequestId(UUID.randomUUID().toString()).build();
    serverJobId = UUID.randomUUID().toString();
  }

  @Test
  public void pushJobThenPull() throws Exception {
    // no setup

    pubSubJobQueue.sendJob(jobKey, serverJobId);
    Optional<JobQueueItem> jobQueueItem = pubSubJobQueue.receiveJob();

    assertThat(jobQueueItem).isPresent();
    assertThat(jobQueueItem.get().getJobKeyString()).isEqualTo(jobKey.getJobRequestId());
  }

  @Test
  public void receiveJob_emptyQueueYieldsEmptyReceive() throws Exception {
    // no setup

    Optional<JobQueueItem> jobQueueItem = pubSubJobQueue.receiveJob();

    assertThat(jobQueueItem).isEmpty();
  }

  @Test
  public void modifyJobProcessingTime_modifyAfterReceive() throws JobQueueException {
    pubSubJobQueue.sendJob(jobKey, serverJobId);
    Optional<JobQueueItem> jobQueueItem = pubSubJobQueue.receiveJob();

    assertThat(jobQueueItem).isPresent();
    assertThat(jobQueueItem.get().getJobKeyString()).isEqualTo(jobKey.getJobRequestId());

    pubSubJobQueue.modifyJobProcessingTime(jobQueueItem.get(), Duration.ofMinutes(5));
  }

  @Test
  public void acknowledgeJobCompletion_exceptionOnNonExistentItem() throws Exception {
    JobQueueItem nonExistentItem =
        JobQueueItem.newBuilder()
            .setJobKeyString("foo|bar.com")
            .setJobProcessingTimeout(Durations.fromSeconds(1))
            .setJobProcessingStartTime(ProtoUtil.toProtoTimestamp(Instant.now()))
            .setReceiptInfo("thisDoesntExist")
            .build();

    assertThrows(
        JobQueueException.class, () -> pubSubJobQueue.acknowledgeJobCompletion(nonExistentItem));
  }

  /** Full queue functionality test. Pushes, pulls, and acknowledges that the item was processed. */
  @Test
  public void pushJobThenPullThenAck() throws Exception {
    // Push to the queue
    pubSubJobQueue.sendJob(jobKey, serverJobId);

    // Pull the item back from the queue, check that it is the correct item
    Optional<JobQueueItem> jobQueueItem = pubSubJobQueue.receiveJob();
    assertThat(jobQueueItem).isPresent();
    assertThat(jobQueueItem.get().getJobKeyString()).isEqualTo(jobKey.getJobRequestId());

    // Acknowledge processing of the item (deleting it from the queue)
    pubSubJobQueue.acknowledgeJobCompletion(jobQueueItem.get());

    // Check that the queue is empty
    jobQueueItem = pubSubJobQueue.receiveJob();
    assertThat(jobQueueItem).isEmpty();
  }
}
