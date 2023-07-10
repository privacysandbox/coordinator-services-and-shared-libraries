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
import com.amazonaws.services.lambda.runtime.RequestHandler;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.aws.gateway.error.ErrorReasons;
import com.google.scp.shared.aws.gateway.error.ErrorResponse;
import com.google.scp.shared.aws.gateway.error.ErrorUtil;
import com.google.scp.shared.aws.gateway.injection.factories.FrontendServicesFactory;
import com.google.scp.shared.aws.gateway.serialization.SerializerFacade;
import com.google.scp.shared.aws.gateway.serialization.SerializerFacadeException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * BaseApiGatewayEvent instruments request forwarding, response serving and exception handling for
 * sub classes that wish to expose API Gateway event handlers.
 *
 * @param <TResponse> The data type of the response object that this endpoint returns.
 */
public abstract class BaseApiGatewayEvent<TResponse>
    implements RequestHandler<APIGatewayProxyRequestEvent, APIGatewayProxyResponseEvent> {

  protected final SerializerFacade jsonSerializerFacade;
  protected final Logger logger;
  private APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent;

  public BaseApiGatewayEvent() {
    jsonSerializerFacade = FrontendServicesFactory.getJsonSerializer();
    logger = LoggerFactory.getLogger(getClass());
  }

  /** This is called by the AWS runtime when an API Gateway request is invoked */
  public final APIGatewayProxyResponseEvent handleRequest(
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent, Context context) {
    try {
      this.apiGatewayProxyRequestEvent = apiGatewayProxyRequestEvent;
      TResponse response = handleRequestEvent(apiGatewayProxyRequestEvent, context);
      return handleResponse(response);
    } catch (ServiceException e) {
      return handleException(e);
    } catch (Exception e) {
      return handleException(
          new ServiceException(
              Code.UNKNOWN,
              ErrorReasons.SERVER_ERROR.toString(),
              ErrorReasons.SERVER_ERROR.toString(),
              e));
    }
  }

  /** Returns the original APIGatewayProxyRequestEvent that was passed in by API Gateway */
  protected APIGatewayProxyRequestEvent getApiGatewayProxyRequestEvent() {
    return apiGatewayProxyRequestEvent;
  }

  /** This is overriden by subclasses to handle the actual request event from API Gateway */
  protected abstract TResponse handleRequestEvent(
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent, Context context) throws Exception;

  /**
   * Serializes the response and returns the corresponding APIGatewayProxyResponseEvent to be
   * returned to API Gateway
   */
  protected APIGatewayProxyResponseEvent handleResponse(TResponse response)
      throws ServiceException, SerializerFacadeException {
    APIGatewayProxyResponseEvent apiGatewayProxyResponseEvent = new APIGatewayProxyResponseEvent();
    apiGatewayProxyResponseEvent.setStatusCode(Code.OK.getHttpStatusCode());
    apiGatewayProxyResponseEvent.setBody(jsonSerializerFacade.serialize(response));
    return onServeResponse(apiGatewayProxyResponseEvent);
  }

  /**
   * Notifies subclasses that override this method that the response is about to be served and
   * allows them to inspect, modify or replace the actual response.
   */
  protected APIGatewayProxyResponseEvent onServeResponse(
      APIGatewayProxyResponseEvent responseEvent) {
    return responseEvent;
  }

  /**
   * Handles ServiceExceptions by returning a corresponding APIGatewayProxyResponseEvent with the
   * exception in the response
   */
  protected APIGatewayProxyResponseEvent handleException(ServiceException exception) {
    logger.error("Handling ServiceException: " + exception);
    APIGatewayProxyResponseEvent apiGatewayProxyResponseEvent = new APIGatewayProxyResponseEvent();
    apiGatewayProxyResponseEvent.setStatusCode(exception.getErrorCode().getHttpStatusCode());
    ErrorResponse errorResponse = ErrorUtil.toErrorResponse(exception);
    String errorResponseJson;
    try {
      errorResponseJson = jsonSerializerFacade.serialize(errorResponse);
    } catch (SerializerFacadeException e) {
      // This should never be returned
      errorResponseJson = "{}";
    }
    apiGatewayProxyResponseEvent.setBody(errorResponseJson);
    return onServeResponse(apiGatewayProxyResponseEvent);
  }
}
