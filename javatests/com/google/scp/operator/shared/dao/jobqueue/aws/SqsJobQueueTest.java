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

package com.google.scp.operator.shared.dao.jobqueue.aws;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.common.base.Supplier;
import com.google.common.base.Suppliers;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Provides;
import com.google.protobuf.util.Durations;
import com.google.scp.operator.protos.shared.backend.JobKeyProto.JobKey;
import com.google.scp.operator.protos.shared.backend.jobqueue.JobQueueProto.JobQueueItem;
import com.google.scp.operator.shared.dao.jobqueue.aws.SqsJobQueue.JobQueueSqsMaxWaitTimeSeconds;
import com.google.scp.operator.shared.dao.jobqueue.aws.SqsJobQueue.JobQueueSqsQueueUrl;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue.JobQueueException;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue.JobQueueMessageLeaseSeconds;
import com.google.scp.shared.proto.ProtoUtil;
import com.google.scp.shared.testutils.aws.AwsHermeticTestHelper;
import java.time.Instant;
import java.util.Optional;
import java.util.UUID;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.testcontainers.containers.localstack.LocalStackContainer;
import org.testcontainers.containers.wait.strategy.Wait;
import software.amazon.awssdk.services.sqs.SqsClient;

/** Hermetic Test for the SqsJobQueue. */
@RunWith(JUnit4.class)
public class SqsJobQueueTest {

  @Rule public Acai acai = new Acai(TestEnv.class);

  // Class rule for hermetic testing.
  private static final Supplier<LocalStackContainer> memoizedContainerSupplier =
      Suppliers.memoize(
          () -> AwsHermeticTestHelper.createContainer(LocalStackContainer.Service.SQS));
  @ClassRule public static LocalStackContainer localstack = memoizedContainerSupplier.get();

  // Under test
  @Inject SqsJobQueue sqsJobQueue;

  JobKey jobKey;
  String serverJobId;

  @Before
  public void setUp() throws JobQueueException {
    jobKey = JobKey.newBuilder().setJobRequestId(UUID.randomUUID().toString()).build();
    serverJobId = UUID.randomUUID().toString();

    localstack.waitingFor(Wait.forHealthcheck());
  }

  /**
   * End to end test of queue functionality. Inserts, reads back, and acknowledges that the item was
   * processed.
   */
  @Test
  public void testDoubleInsertThenReadThenAck() throws Exception {
    // Write to the queue twice, the queue should deduplicate these
    sqsJobQueue.sendJob(jobKey, serverJobId);

    // Read the item back from the queue, check that it is the correct item
    Optional<JobQueueItem> jobQueueItem = sqsJobQueue.receiveJob();
    assertThat(jobQueueItem).isPresent();
    assertThat(jobQueueItem.get().getJobKeyString()).isEqualTo(jobKey.getJobRequestId());

    // Acknowledge processing of the item (deleting it from the queue)
    sqsJobQueue.acknowledgeJobCompletion(jobQueueItem.get());

    // Check that the queue is empty
    jobQueueItem = sqsJobQueue.receiveJob();
    assertThat(jobQueueItem).isEmpty();
  }

  /** Test that an empty optional is returned if receiveJob is called on an empty queue */
  @Test
  public void testEmptyQueueYieldsEmptyReceive() throws Exception {
    Optional<JobQueueItem> jobQueueItem = sqsJobQueue.receiveJob();
    assertThat(jobQueueItem).isEmpty();
  }

  /** Test that an exception is thrown if a non-existent queue item is acknowledged */
  @Test
  public void testExceptionOnAckMissing() throws Exception {
    JobQueueItem nonExistentItem =
        JobQueueItem.newBuilder()
            .setJobKeyString("foo|bar.com")
            .setJobProcessingTimeout(Durations.fromSeconds(10))
            .setJobProcessingStartTime(ProtoUtil.toProtoTimestamp(Instant.now()))
            .setReceiptInfo("thisDoesntExist")
            .build();
    assertThrows(
        JobQueueException.class, () -> sqsJobQueue.acknowledgeJobCompletion(nonExistentItem));
  }

  public static final class TestEnv extends AbstractModule {

    @Override
    public void configure() {
      bind(String.class)
          .annotatedWith(JobQueueSqsQueueUrl.class)
          .toInstance(AwsHermeticTestHelper.getSqsQueueUrlDefaultName());
      bind(int.class).annotatedWith(JobQueueSqsMaxWaitTimeSeconds.class).toInstance(2);
      bind(int.class)
          .annotatedWith(JobQueueMessageLeaseSeconds.class)
          .toInstance(60 * 60 * 2); // two hours
      // Bind container provided by helper class. .
      bind(LocalStackContainer.class).toInstance(memoizedContainerSupplier.get());
    }

    @Provides
    SqsClient provideSqsClient(LocalStackContainer localstack) {
      SqsClient sqsClient = AwsHermeticTestHelper.createSqsClient(localstack);
      AwsHermeticTestHelper.createSqsQueueDefaultName(sqsClient);
      return sqsClient;
    }
  }
}
