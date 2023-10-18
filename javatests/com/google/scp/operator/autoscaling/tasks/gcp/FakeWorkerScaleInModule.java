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

package com.google.scp.operator.autoscaling.tasks.gcp;

import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.operator.autoscaling.tasks.gcp.Annotations.TerminationWaitTimeout;

/** WorkerScaleInModule used for testing. */
public class FakeWorkerScaleInModule extends AbstractModule {

  @Provides
  @TerminationWaitTimeout
  public Integer getTerminationWaitTimeout() {
    return 10;
  }

  @Override
  protected void configure() {
    bind(GcpInstanceManagementConfig.class)
        .toInstance(
            GcpInstanceManagementConfig.builder()
                .setRegion("us-central1")
                .setProjectId("fakeproject")
                .setManagedInstanceGroupName("test-mig")
                .build());
  }
}
