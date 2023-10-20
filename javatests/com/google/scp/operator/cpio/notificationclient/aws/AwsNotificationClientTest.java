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

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import com.google.scp.operator.cpio.notificationclient.NotificationClient;
import com.google.scp.operator.cpio.notificationclient.NotificationClient.NotificationClientException;
import com.google.scp.operator.cpio.notificationclient.model.PublishMessageRequest;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import software.amazon.awssdk.services.sns.SnsClient;
import software.amazon.awssdk.services.sns.model.PublishRequest;
import software.amazon.awssdk.services.sns.model.PublishResponse;
import software.amazon.awssdk.services.sns.model.SnsException;

@RunWith(JUnit4.class)
public final class AwsNotificationClientTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private static SnsClient snsClientSuccess;
  @Mock private static SnsClient snsClientException;

  /** Tests the NotificationClient happy path. */
  @Test
  public void publishMessage_success() throws Exception {
    PublishResponse publishResponseSuccess = PublishResponse.builder().messageId("1").build();
    PublishMessageRequest publishMessageRequest =
        PublishMessageRequest.builder()
            .setMessageBody("HelloWorld")
            .setNotificationTopic("OutterSpace")
            .build();
    NotificationClient notificationClientSuccess = new AwsNotificationClient(snsClientSuccess);
    when(snsClientSuccess.publish(any(PublishRequest.class))).thenReturn(publishResponseSuccess);
    notificationClientSuccess.publishMessage(publishMessageRequest);
  }

  /** Tests the NotificationClient exception code path. */
  @Test(expected = NotificationClientException.class)
  public void publishMessage_notificationClientException() throws Exception {
    SnsException snsException = (SnsException) SnsException.builder().message("Message").build();
    PublishMessageRequest publishMessageRequest =
        PublishMessageRequest.builder()
            .setMessageBody("HelloWorld")
            .setNotificationTopic("OutterSpace")
            .build();
    NotificationClient notificationClientException = new AwsNotificationClient(snsClientException);
    when(snsClientException.publish(any(PublishRequest.class))).thenThrow(snsException);
    notificationClientException.publishMessage(publishMessageRequest);
  }
}
