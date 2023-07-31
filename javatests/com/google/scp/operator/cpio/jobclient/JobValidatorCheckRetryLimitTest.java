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

package com.google.scp.operator.cpio.jobclient;

import static com.google.common.truth.Truth.assertThat;

import com.google.acai.Acai;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.jobclient.JobHandlerModule.JobClientJobMaxNumAttemptsBinding;
import com.google.scp.operator.cpio.jobclient.model.Job;
import com.google.scp.operator.cpio.jobclient.testing.FakeJobGenerator;
import java.time.Clock;
import java.time.Instant;
import java.time.ZoneId;
import java.util.Optional;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

public final class JobValidatorCheckRetryLimitTest {

  @Rule public Acai acai = new Acai(TestEnv.class);

  private Job baseJob;
  @Inject private JobValidatorCheckRetryLimit jobValidatorCheckRetryLimit;

  @Before
  public void setup() {
    baseJob = FakeJobGenerator.generate("foo");
  }

  @Test
  public void validate_notExhaustedRetryLimitReturnsTrue() {
    Job job = baseJob.toBuilder().setNumAttempts(1).build();

    assertThat(jobValidatorCheckRetryLimit.validate(Optional.of(job), "not_used")).isEqualTo(true);
  }

  @Test
  public void validate_exhaustedRetryLimitReturnsFalse() {
    Job job = baseJob.toBuilder().setNumAttempts(2).build();

    assertThat(jobValidatorCheckRetryLimit.validate(Optional.of(job), "not_used")).isEqualTo(false);
  }

  static class TestEnv extends AbstractModule {

    @Override
    protected final void configure() {
      bind(Integer.class).annotatedWith(JobClientJobMaxNumAttemptsBinding.class).toInstance(2);
      install(new JobValidatorModule());
    }

    @Provides
    @Singleton
    Clock provideClock() {
      return Clock.fixed(Instant.parse("2021-01-01T12:30:00Z"), ZoneId.systemDefault());
    }
  }
}
