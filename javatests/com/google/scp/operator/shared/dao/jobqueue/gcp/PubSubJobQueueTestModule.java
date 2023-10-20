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

import com.google.acai.TestingServiceModule;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.pubsub.v1.ProjectSubscriptionName;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue.JobQueueMessageLeaseSeconds;
import com.google.scp.operator.shared.dao.jobqueue.gcp.PubSubJobQueue.JobQueuePubSubSubscriptionName;
import com.google.scp.shared.testutils.gcp.PubSubEmulatorContainerTestModule;
import com.google.scp.shared.testutils.gcp.PubSubLocalService;

public final class PubSubJobQueueTestModule extends AbstractModule {

  public static final String PROJECT_ID = "projectId";
  public static final String TOPIC_ID = "topicId";
  public static final String SUBSCRIPTION_ID = "jobsSubscription";

  @Override
  public void configure() {
    install(new PubSubEmulatorContainerTestModule(PROJECT_ID, TOPIC_ID, SUBSCRIPTION_ID));
    install(TestingServiceModule.forServices(PubSubLocalService.class));
  }

  @Provides
  @JobQueueMessageLeaseSeconds
  int provideMessageLeaseSeconds() {
    return 10;
  }

  @Provides
  @JobQueuePubSubSubscriptionName
  String providePubSubSubscriptionName() {
    return ProjectSubscriptionName.format(PROJECT_ID, SUBSCRIPTION_ID);
  }
}
