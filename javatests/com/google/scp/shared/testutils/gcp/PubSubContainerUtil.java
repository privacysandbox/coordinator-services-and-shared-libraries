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

package com.google.scp.shared.testutils.gcp;

import com.google.api.gax.core.CredentialsProvider;
import com.google.api.gax.rpc.TransportChannelProvider;
import com.google.cloud.pubsub.v1.Publisher;
import com.google.cloud.pubsub.v1.SubscriptionAdminClient;
import com.google.cloud.pubsub.v1.SubscriptionAdminSettings;
import com.google.cloud.pubsub.v1.TopicAdminClient;
import com.google.cloud.pubsub.v1.TopicAdminSettings;
import com.google.cloud.pubsub.v1.stub.GrpcSubscriberStub;
import com.google.cloud.pubsub.v1.stub.SubscriberStub;
import com.google.cloud.pubsub.v1.stub.SubscriberStubSettings;
import com.google.pubsub.v1.ProjectSubscriptionName;
import com.google.pubsub.v1.PushConfig;
import com.google.pubsub.v1.TopicName;
import java.io.IOException;

/** Util for creating pubsub components. */
public class PubSubContainerUtil {

  public static void createSubscription(
      String projectId,
      String topicId,
      String subscriptionId,
      TransportChannelProvider channelProvider,
      CredentialsProvider credentialsProvider)
      throws IOException {
    SubscriptionAdminSettings subscriptionAdminSettings =
        SubscriptionAdminSettings.newBuilder()
            .setTransportChannelProvider(channelProvider)
            .setCredentialsProvider(credentialsProvider)
            .build();
    SubscriptionAdminClient subscriptionAdminClient =
        SubscriptionAdminClient.create(subscriptionAdminSettings);
    ProjectSubscriptionName subscriptionName =
        ProjectSubscriptionName.of(projectId, subscriptionId);
    subscriptionAdminClient.createSubscription(
        subscriptionName, TopicName.of(projectId, topicId), PushConfig.getDefaultInstance(), 10);
  }

  public static void createTopic(
      String projectId,
      String topicId,
      TransportChannelProvider channelProvider,
      CredentialsProvider credentialsProvider)
      throws IOException {
    TopicAdminSettings topicAdminSettings =
        TopicAdminSettings.newBuilder()
            .setTransportChannelProvider(channelProvider)
            .setCredentialsProvider(credentialsProvider)
            .build();
    try (TopicAdminClient topicAdminClient = TopicAdminClient.create(topicAdminSettings)) {
      TopicName topicName = TopicName.of(projectId, topicId);
      topicAdminClient.createTopic(topicName);
    }
  }

  public static SubscriberStub createSubscriptionStub(
      TransportChannelProvider channelProvider, CredentialsProvider credentialsProvider)
      throws IOException {

    SubscriberStubSettings subscriberStubSettings =
        SubscriberStubSettings.newBuilder()
            .setTransportChannelProvider(channelProvider)
            .setCredentialsProvider(credentialsProvider)
            .build();

    return GrpcSubscriberStub.create(subscriberStubSettings);
  }

  public static Publisher createPublisher(
      String projectId,
      String topicId,
      TransportChannelProvider channelProvider,
      CredentialsProvider credentialsProvider)
      throws IOException {
    return Publisher.newBuilder(TopicName.of(projectId, topicId))
        .setChannelProvider(channelProvider)
        .setCredentialsProvider(credentialsProvider)
        .build();
  }
}
