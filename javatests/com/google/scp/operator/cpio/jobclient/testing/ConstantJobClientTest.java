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

package com.google.scp.operator.cpio.jobclient.testing;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.scp.operator.cpio.jobclient.JobClient.JobClientException;
import com.google.scp.operator.cpio.jobclient.model.Job;
import com.google.scp.operator.cpio.jobclient.model.JobResult;
import java.util.Optional;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class ConstantJobClientTest {

  private ConstantJobClient jobClient;

  @Before
  public void setUp() {
    jobClient = new ConstantJobClient();
  }

  @Test
  public void pullDefaultEmpty() throws Exception {
    Optional<Job> jobPulled = jobClient.getJob();

    assertThat(jobPulled).isEmpty();
  }

  @Test
  public void pullConstantAndExhaust() throws Exception {
    Job job = FakeJobGenerator.generate("foo");
    jobClient.setReturnConstant(job);

    Optional<Job> jobPulled = jobClient.getJob();

    assertThat(jobPulled).isPresent();
    assertThat(jobPulled.get()).isEqualTo(job);

    Optional<Job> exhaustedItem = jobClient.getJob();

    assertThat(exhaustedItem).isEmpty();
  }

  @Test
  public void pullEmptyExplicit() throws Exception {
    Job job = FakeJobGenerator.generate("foo");
    jobClient.setReturnConstant(job);
    jobClient.setReturnEmpty();

    Optional<Job> jobPulled = jobClient.getJob();

    assertThat(jobPulled).isEmpty();
  }

  @Test
  public void testMarkJobCompleted() throws Exception {
    Job job = FakeJobGenerator.generate("foo");
    JobResult jobResult = FakeJobResultGenerator.fromJob(job);

    jobClient.markJobCompleted(jobResult);

    assertThat(jobClient.getLastJobResultCompleted()).isEqualTo(jobResult);
  }

  /** Test that getJob throws an exception when set to but does not throw on successive calls */
  @Test
  public void testGetJobThrowsWhenSetTo() throws Exception {
    Job job = FakeJobGenerator.generate("fizz");
    jobClient.setShouldThrowOnGetJob(true);
    jobClient.setReturnConstant(job);

    // Should throw on first call, but not the second one
    assertThrows(JobClientException.class, () -> jobClient.getJob());
    Optional<Job> jobPulled = jobClient.getJob();

    assertThat(jobPulled).hasValue(job);
  }

  @Test
  public void testMarkJobCompletedThrowsWhenSetTo() throws Exception {
    Job job = FakeJobGenerator.generate("foo");
    JobResult jobResult = FakeJobResultGenerator.fromJob(job);
    jobClient.setShouldThrowOnMarkJobCompleted(true);

    assertThrows(JobClientException.class, () -> jobClient.markJobCompleted(jobResult));
  }
}
