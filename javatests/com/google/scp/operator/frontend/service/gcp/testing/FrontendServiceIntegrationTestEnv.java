/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.operator.frontend.service.gcp.testing;

import static com.google.scp.operator.shared.dao.jobqueue.gcp.PubSubJobQueueTestModule.PROJECT_ID;
import static com.google.scp.operator.shared.dao.jobqueue.gcp.PubSubJobQueueTestModule.SUBSCRIPTION_ID;

import com.google.cloud.pubsub.v1.Publisher;
import com.google.cloud.pubsub.v1.stub.SubscriberStub;
import com.google.cloud.spanner.DatabaseClient;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.pubsub.v1.ProjectSubscriptionName;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue.JobQueueMessageLeaseSeconds;
import com.google.scp.operator.shared.dao.jobqueue.gcp.PubSubJobQueue.JobQueuePubSubSubscriptionName;
import com.google.scp.operator.shared.dao.jobqueue.gcp.PubSubJobQueueTestModule;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobMetadataDbClient;
import com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerMetadataDb.MetadataDbSpannerTtlDays;
import com.google.scp.operator.shared.dao.metadatadb.gcp.SpannerMetadataDbTestModule;
import com.google.scp.shared.testutils.gcp.PubSubEmulatorContainer;
import com.google.scp.shared.testutils.gcp.SpannerEmulatorContainer;

/** Test environment for frontend service integration test */
public class FrontendServiceIntegrationTestEnv extends AbstractModule {
  private Injector spannerInjector;
  private Injector pubSubInjector;

  @Provides
  @Singleton
  public SpannerEmulatorContainer provideSpannerEmulatorContainer() {
    return spannerInjector.getInstance(SpannerEmulatorContainer.class);
  }

  @Provides
  @Singleton
  public PubSubEmulatorContainer providePubSubEmulatorContainer() {
    return pubSubInjector.getInstance(PubSubEmulatorContainer.class);
  }

  @Override
  public void configure() {
    spannerInjector = Guice.createInjector(new SpannerMetadataDbTestModule());
    spannerInjector.getInstance(SpannerEmulatorContainer.class).start();
    pubSubInjector = Guice.createInjector(new PubSubJobQueueTestModule());
    pubSubInjector.getInstance(PubSubEmulatorContainer.class).start();

    bind(DatabaseClient.class)
        .annotatedWith(JobMetadataDbClient.class)
        .toInstance(spannerInjector.getInstance(DatabaseClient.class));
    bind(Publisher.class).toInstance(pubSubInjector.getInstance(Publisher.class));
    bind(SubscriberStub.class).toInstance(pubSubInjector.getInstance(SubscriberStub.class));
    bind(int.class).annotatedWith(JobQueueMessageLeaseSeconds.class).toInstance(10);
    bind(String.class)
        .annotatedWith(JobQueuePubSubSubscriptionName.class)
        .toInstance(ProjectSubscriptionName.format(PROJECT_ID, SUBSCRIPTION_ID));
    bind(int.class).annotatedWith(MetadataDbSpannerTtlDays.class).toInstance(1);

    install(new FrontendserviceCloudFunctinEmulatorContainer());
  }
}
