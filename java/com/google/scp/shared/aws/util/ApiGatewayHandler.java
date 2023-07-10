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

package com.google.scp.shared.aws.util;

import static com.google.scp.shared.api.util.ErrorUtil.toErrorResponse;
import static com.google.scp.shared.aws.util.LambdaHandlerUtil.createApiGatewayResponseFromProto;

import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.RequestHandler;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.common.collect.ImmutableMap;
import com.google.protobuf.util.JsonFormat;
import com.google.protobuf.util.JsonFormat.Parser;
import com.google.scp.shared.api.exception.ServiceException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/* Base class for Api Gateway Handlers. */
public abstract class ApiGatewayHandler<Request, Response>
    implements RequestHandler<APIGatewayProxyRequestEvent, APIGatewayProxyResponseEvent> {

  protected static final Logger logger = LoggerFactory.getLogger(ApiGatewayHandler.class);

  protected final Parser parser = JsonFormat.parser();

  public ApiGatewayHandler() {}

  /** Converts api gateway request to request model. */
  protected abstract Request toRequest(
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent, Context context)
      throws ServiceException;

  /**
   * Authorizes the request. Simply returns void if the request authorization passes. Throws a
   * ServiceException if an error occurs.
   *
   * <p>Unless overridden this does nothing, meaning that unless a subclass implements this then
   * authorization is disabled. This should be overridden in subclasses that need authorization.
   */
  protected void authorizeRequest(
      Request request, APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent)
      throws ServiceException {}

  /** Processes given request and returns response. */
  protected abstract Response processRequest(Request request) throws ServiceException;

  /** Converts response model to api gateway response. Can be overridden in subclass. */
  // TODO: Add default implementation to convert proto response to JSON, once all Response models
  //   are in proto. At time of this comment, only ConsumePrivacyBudgetResponse remains.
  protected abstract APIGatewayProxyResponseEvent toApiGatewayResponse(Response response);

  /** Handles incoming request. */
  @Override
  public APIGatewayProxyResponseEvent handleRequest(
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent, Context context) {
    try {
      Request request = toRequest(apiGatewayProxyRequestEvent, context);
      authorizeRequest(request, apiGatewayProxyRequestEvent);
      return toApiGatewayResponse(processRequest(request));
    } catch (ServiceException exception) {
      logger.warn("ServiceException occurred during handling, returning error response", exception);
      return createApiGatewayResponseFromProto(
          /* bodyObject = */ toErrorResponse(exception),
          /* httpStatusCode = */ exception.getErrorCode().getHttpStatusCode(),
          /* headers = */ allHeaders());
    } catch (Exception exception) {
      ServiceException serviceException = ServiceException.ofUnknownException(exception);
      logger.warn("Exception occurred during handling, returning error response", serviceException);
      return createApiGatewayResponseFromProto(
          /* bodyObject = */ toErrorResponse(serviceException),
          /* httpStatusCode = */ serviceException.getErrorCode().getHttpStatusCode(),
          /* headers = */ allHeaders());
    }
  }

  /** Override the below if subclass needs to add custom header */
  protected ImmutableMap<String, String> customHeaders() {
    return ImmutableMap.of();
  }

  protected final ImmutableMap<String, String> allHeaders() {
    return new ImmutableMap.Builder<String, String>()
        .putAll(customHeaders())
        .put("content-type", "application/json")
        .build();
  }
}
