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

package com.google.scp.shared.aws.gateway;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.aws.gateway.ResponseEventAssertions.assertThatResponseBodyContains;
import static com.google.scp.shared.aws.gateway.ResponseEventAssertions.assertThatResponseBodyDoesNotContain;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.aws.gateway.error.ErrorResponse;
import com.google.scp.shared.aws.gateway.error.ErrorResponse.Details;
import com.google.scp.shared.aws.gateway.testing.TestCreateResponse;
import java.util.List;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Tests the abstract class BaseApiGatewayEvent via the TestBaseApiGatewayEvent subclass */
@RunWith(JUnit4.class)
public class BaseApiGatewayEventTest {

  private FakeBaseApiGatewayEvent target;
  private TestCreateResponse testCreateResponse;
  private APIGatewayProxyRequestEvent requestEvent;

  @Before
  public void setUp() {
    testCreateResponse = TestCreateResponse.builder().ackId("123").build();
    requestEvent = new APIGatewayProxyRequestEvent();
  }

  @Test
  public void responseReturned_whenRequestIsReceived() throws JsonProcessingException {
    target = new FakeBaseApiGatewayEvent(testCreateResponse);

    APIGatewayProxyResponseEvent responseEvent = target.handleRequest(requestEvent, null);

    assertThatResponseBodyContains(responseEvent, testCreateResponse);
    assertThat(requestEvent).isEqualTo(target.getRequestEvent());
  }

  @Test
  public void responseReplaced_whenOnServeResponseReplacesResponse()
      throws JsonProcessingException {
    APIGatewayProxyResponseEvent responseEvent = new APIGatewayProxyResponseEvent();
    target = new FakeBaseApiGatewayEvent(testCreateResponse, responseEvent);

    APIGatewayProxyResponseEvent responseEventResult = target.handleRequest(requestEvent, null);

    assertThatResponseBodyDoesNotContain(responseEvent, testCreateResponse);
    assertThat(requestEvent).isEqualTo(target.getRequestEvent());
    assertThat(responseEventResult).isEqualTo(responseEvent);
  }

  @Test
  public void unknownErrorResponseReturned_whenExceptionThrownDuringHandleRequest()
      throws JsonProcessingException {
    target = new FakeBaseApiGatewayEvent(new RuntimeException());

    APIGatewayProxyResponseEvent responseEventResult = target.handleRequest(requestEvent, null);

    assertThatResponseBodyContains(
        responseEventResult, UnknownErrorResponse.getUnknownErrorResponse());
    assertThat(requestEvent).isEqualTo(target.getRequestEvent());
    assertThat(responseEventResult.getStatusCode()).isEqualTo(Code.UNKNOWN.getHttpStatusCode());
  }

  @Test
  public void knownErrorResponseReturned_whenServiceExceptionThrownDuringHandleRequest()
      throws JsonProcessingException {
    String reason = "Response aborted";
    String message = "Response was aborted";
    ServiceException serviceException = new ServiceException(Code.ABORTED, reason, message);
    target = new FakeBaseApiGatewayEvent(serviceException);

    APIGatewayProxyResponseEvent responseEventResult = target.handleRequest(requestEvent, null);

    ErrorResponse errorResponse =
        ErrorResponse.builder()
            .setCode(Code.ABORTED.getRpcStatusCode())
            .setMessage(message)
            .setDetails(List.of(Details.builder().setReason(reason).build()))
            .build();

    assertThatResponseBodyContains(responseEventResult, errorResponse);
    assertThat(requestEvent).isEqualTo(target.getRequestEvent());
    assertThat(responseEventResult.getStatusCode()).isEqualTo(Code.ABORTED.getHttpStatusCode());
  }

  @Test
  public void
      replaceResponseReturned_whenServiceExceptionThrownDuringHandleRequestButResponseReplaced() {
    String reason = "Response aborted";
    String message = "Response was aborted";
    ServiceException serviceException = new ServiceException(Code.ABORTED, reason, message);
    APIGatewayProxyResponseEvent responseEvent = new APIGatewayProxyResponseEvent();
    target = new FakeBaseApiGatewayEvent(serviceException, responseEvent);

    APIGatewayProxyResponseEvent responseEventResult = target.handleRequest(requestEvent, null);

    assertThat(responseEventResult).isEqualTo(responseEvent);
    assertThat(requestEvent).isEqualTo(target.getRequestEvent());
  }
}
