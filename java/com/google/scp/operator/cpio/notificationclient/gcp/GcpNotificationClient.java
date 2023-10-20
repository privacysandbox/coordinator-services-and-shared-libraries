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

import com.google.api.gax.rpc.ApiException;
import com.google.cloud.pubsub.v1.stub.PublisherStub;
import com.google.inject.Inject;
import com.google.protobuf.ByteString;
import com.google.pubsub.v1.PublishRequest;
import com.google.pubsub.v1.PublishResponse;
import com.google.pubsub.v1.PubsubMessage;
import com.google.scp.operator.cpio.notificationclient.NotificationClient;
import com.google.scp.operator.cpio.notificationclient.gcp.Annotations.GcpNotificationClientPublisher;
import com.google.scp.operator.cpio.notificationclient.model.PublishMessageRequest;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** GCP implementation of {@code NotificationClient}. */
public final class GcpNotificationClient implements NotificationClient {

  private static final Logger logger = LoggerFactory.getLogger(GcpNotificationClient.class);
  private final PublisherStub publisher;

  @Inject
  GcpNotificationClient(@GcpNotificationClientPublisher PublisherStub publisher) {
    this.publisher = publisher;
  }

  @Override
  public void publishMessage(PublishMessageRequest publishMessageRequest)
      throws NotificationClientException {
    try {
      ByteString data = ByteString.copyFromUtf8(publishMessageRequest.messageBody());
      PubsubMessage pubsubMessage =
          PubsubMessage.newBuilder()
              .setData(data)
              .putAllAttributes(publishMessageRequest.messageAttributes())
              .build();

      PublishRequest publishRequest =
          PublishRequest.newBuilder()
              .addMessages(pubsubMessage)
              .setTopic(publishMessageRequest.notificationTopic())
              .build();
      PublishResponse publishResponse = publisher.publishCallable().call(publishRequest);
      logger.info(
          "Published messages: "
              + publishResponse.getMessageIdsList()
              + " to: "
              + publishMessageRequest.notificationTopic());
    } catch (ApiException e) {
      throw new NotificationClientException(e);
    }
  }
}
