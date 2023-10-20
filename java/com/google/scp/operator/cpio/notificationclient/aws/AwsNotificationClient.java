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

package com.google.scp.operator.cpio.notificationclient.aws;

import com.google.inject.Inject;
import com.google.scp.operator.cpio.notificationclient.NotificationClient;
import com.google.scp.operator.cpio.notificationclient.model.PublishMessageRequest;
import java.util.HashMap;
import java.util.Map;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import software.amazon.awssdk.services.sns.SnsClient;
import software.amazon.awssdk.services.sns.model.MessageAttributeValue;
import software.amazon.awssdk.services.sns.model.PublishRequest;
import software.amazon.awssdk.services.sns.model.PublishResponse;
import software.amazon.awssdk.services.sns.model.SnsException;

/** AWS implementation of {@code NotificationClient}. */
public final class AwsNotificationClient implements NotificationClient {

  private static final Logger logger = LoggerFactory.getLogger(AwsNotificationClient.class);
  private final SnsClient snsClient;

  @Inject
  AwsNotificationClient(SnsClient snsClient) {
    this.snsClient = snsClient;
  }

  @Override
  public void publishMessage(PublishMessageRequest publishMessageRequest)
      throws NotificationClientException {
    try {
      String data = publishMessageRequest.messageBody();
      Map<String, MessageAttributeValue> attributes = new HashMap<>();
      for (Map.Entry<String, String> entry : publishMessageRequest.messageAttributes().entrySet()) {
        attributes.put(
            entry.getKey(),
            MessageAttributeValue.builder()
                .dataType("String")
                .stringValue(entry.getValue())
                .build());
      }

      PublishRequest publishRequest =
          PublishRequest.builder()
              .message(data)
              .topicArn(publishMessageRequest.notificationTopic())
              .messageAttributes(attributes)
              .build();

      PublishResponse publishResponse = snsClient.publish(publishRequest);
      logger.info(
          "Published message ID "
              + publishResponse.messageId()
              + " to: "
              + publishMessageRequest.notificationTopic());
    } catch (SnsException e) {
      throw new NotificationClientException(e);
    }
  }
}
