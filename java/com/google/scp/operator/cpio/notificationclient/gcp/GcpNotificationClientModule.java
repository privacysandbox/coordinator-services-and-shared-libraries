/*
 * Copyright 2023 Google LLC
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

package com.google.scp.operator.cpio.notificationclient.gcp;

import com.google.cloud.pubsub.v1.stub.GrpcPublisherStub;
import com.google.cloud.pubsub.v1.stub.PublisherStub;
import com.google.cloud.pubsub.v1.stub.PublisherStubSettings;
import com.google.inject.Provides;
import com.google.scp.operator.cpio.notificationclient.NotificationClient;
import com.google.scp.operator.cpio.notificationclient.NotificationClientModule;
import com.google.scp.operator.cpio.notificationclient.gcp.Annotations.GcpNotificationClientPublisher;
import java.io.IOException;

/** Defines dependencies to be used in the GCP Notification Client. */
public final class GcpNotificationClientModule extends NotificationClientModule {

  /**
   * Gets the GCP implementation of {@code NotificationClient}.
   *
   * @return GcpNotificationClient
   */
  @Override
  public Class<? extends NotificationClient> getNotificationClientImpl() {
    return GcpNotificationClient.class;
  }

  /** Provides an instance of the {@code PublisherStub} class. */
  @Provides
  @GcpNotificationClientPublisher
  PublisherStub providePublisher() throws IOException {
    PublisherStubSettings.Builder settingsBuilder = PublisherStubSettings.newBuilder();
    settingsBuilder.setTransportChannelProvider(
        PublisherStubSettings.defaultGrpcTransportProviderBuilder().build());

    return GrpcPublisherStub.create(settingsBuilder.build());
  }
}
