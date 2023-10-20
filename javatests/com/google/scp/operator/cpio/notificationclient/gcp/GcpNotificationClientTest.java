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

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import com.google.api.gax.grpc.GrpcStatusCode;
import com.google.api.gax.rpc.ApiException;
import com.google.api.gax.rpc.UnaryCallable;
import com.google.cloud.pubsub.v1.stub.PublisherStub;
import com.google.pubsub.v1.PublishRequest;
import com.google.pubsub.v1.PublishResponse;
import com.google.scp.operator.cpio.notificationclient.NotificationClient;
import com.google.scp.operator.cpio.notificationclient.NotificationClient.NotificationClientException;
import com.google.scp.operator.cpio.notificationclient.model.PublishMessageRequest;
import io.grpc.Status.Code;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class GcpNotificationClientTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private static PublisherStub publisherSuccess;
  @Mock private static PublisherStub publisherException;
  @Mock private UnaryCallable<PublishRequest, PublishResponse> unaryCallableSuccess;
  @Mock private UnaryCallable<PublishRequest, PublishResponse> unaryCallableException;

  /** Tests the NotificationClient happy path. */
  @Test
  public void publishMessage_success() throws Exception {
    PublishResponse publishResponseSuccess =
        PublishResponse.newBuilder().addMessageIds("1").build();
    when(publisherSuccess.publishCallable()).thenReturn(unaryCallableSuccess);
    when(unaryCallableSuccess.call(any())).thenReturn(publishResponseSuccess);
    NotificationClient notificationClientSuccess = new GcpNotificationClient(publisherSuccess);
    PublishMessageRequest publishMessageRequest =
        PublishMessageRequest.builder()
            .setMessageBody("HelloWorld")
            .setNotificationTopic("OutterSpace")
            .build();
    notificationClientSuccess.publishMessage(publishMessageRequest);
  }

  /** Tests the NotificationClient exception code path. */
  @Test(expected = NotificationClientException.class)
  public void publishMessage_notificationClientException() throws Exception {
    ApiException apiExceptione =
        new ApiException(
            new RuntimeException("Forced ApiException exception triggered."),
            GrpcStatusCode.of(Code.CANCELLED),
            false);
    when(publisherException.publishCallable()).thenReturn(unaryCallableException);
    when(unaryCallableException.call(any())).thenThrow(apiExceptione);
    NotificationClient notificationClientException = new GcpNotificationClient(publisherException);
    PublishMessageRequest publishMessageRequest =
        PublishMessageRequest.builder()
            .setMessageBody("HelloWorld")
            .setNotificationTopic("OutterSpace")
            .build();
    notificationClientException.publishMessage(publishMessageRequest);
  }
}
