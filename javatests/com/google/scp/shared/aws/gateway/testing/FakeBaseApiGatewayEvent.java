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

package com.google.scp.shared.aws.gateway.testing;

import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.scp.shared.aws.gateway.events.BaseApiGatewayEvent;

/** Used to test the BaseApiGatewayEvent */
public final class FakeBaseApiGatewayEvent extends BaseApiGatewayEvent<TestCreateResponse> {

  private TestCreateResponse testCreateResponse;
  private APIGatewayProxyResponseEvent responseEvent;
  private APIGatewayProxyRequestEvent requestEvent;
  private Exception throwExceptionOnRequest;

  /** Used to test when a response is returned */
  public FakeBaseApiGatewayEvent(TestCreateResponse testCreateResponse) {
    this.testCreateResponse = testCreateResponse;
  }

  /**
   * Used to test when the APIGatewayProxyResponseEvent is replaced in the onServeResponse method
   */
  public FakeBaseApiGatewayEvent(
      TestCreateResponse testCreateResponse, APIGatewayProxyResponseEvent responseEvent) {
    this.testCreateResponse = testCreateResponse;
    this.responseEvent = responseEvent;
  }

  /** Used to test when an exception is thrown when the request is handled */
  public FakeBaseApiGatewayEvent(Exception throwExceptionOnRequest) {
    this.throwExceptionOnRequest = throwExceptionOnRequest;
  }

  /**
   * Used to test when an exception is thrown and the APIGatewayProxyResponseEvent is replaced in
   * the onServeResponse method
   */
  public FakeBaseApiGatewayEvent(
      Exception throwExceptionOnRequest, APIGatewayProxyResponseEvent responseEvent) {
    this.throwExceptionOnRequest = throwExceptionOnRequest;
    this.responseEvent = responseEvent;
  }

  @Override
  protected TestCreateResponse handleRequestEvent(
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent, Context context) throws Exception {
    requestEvent = apiGatewayProxyRequestEvent;
    if (throwExceptionOnRequest != null) {
      throw throwExceptionOnRequest;
    }
    return testCreateResponse;
  }

  @Override
  protected APIGatewayProxyResponseEvent onServeResponse(
      APIGatewayProxyResponseEvent responseEvent) {
    if (this.responseEvent == null) {
      this.responseEvent = responseEvent;
    }
    return this.responseEvent;
  }

  public APIGatewayProxyResponseEvent getResponseEvent() {
    return responseEvent;
  }

  public APIGatewayProxyRequestEvent getRequestEvent() {
    return requestEvent;
  }
}
