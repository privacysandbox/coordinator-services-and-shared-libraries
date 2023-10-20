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

package com.google.scp.shared.aws.gateway.events;

import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.aws.gateway.error.ErrorReasons;
import com.google.scp.shared.aws.gateway.serialization.SerializerFacadeException;

/**
 * PostApiGatewayEvent is inherited for classes that wish to expose POST REST endpoints.
 *
 * @param <TRequest> The data type of the request object that is posted to this endpoint.
 * @param <TResponse> The data type of the response object that this endpoint returns.
 */
public abstract class PostApiGatewayEvent<TRequest, TResponse>
    extends BaseApiGatewayEvent<TResponse> {

  private final Class<? extends TRequest> requestClass;

  protected PostApiGatewayEvent(Class<? extends TRequest> requestClass) {
    this.requestClass = requestClass;
  }

  @Override
  protected TResponse handleRequestEvent(
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent, Context context) throws Exception {
    TRequest request;
    try {
      request =
          jsonSerializerFacade.deserialize(apiGatewayProxyRequestEvent.getBody(), requestClass);
    } catch (SerializerFacadeException e) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT, ErrorReasons.JSON_ERROR.toString(), e.getMessage(), e);
    }
    return handlePost(request);
  }

  /** Inherited classes must override this function to receive the deserialized POST request. */
  protected abstract TResponse handlePost(TRequest request) throws Exception;
}
