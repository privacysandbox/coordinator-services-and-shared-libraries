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

package com.google.scp.shared.gcp.util;

import static com.google.scp.shared.api.util.ErrorUtil.toErrorResponse;
import static com.google.scp.shared.gcp.util.CloudFunctionUtil.createCloudFunctionResponseFromProto;
import static java.util.Collections.emptyMap;

import com.google.cloud.functions.HttpFunction;
import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.protobuf.Message;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.exception.ServiceException;
import java.io.IOException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/* Base class for Cloud Function Handlers. */
public abstract class CloudFunctionHandler<Request, Response extends Message>
    implements HttpFunction {

  private static final Logger logger = LoggerFactory.getLogger(CloudFunctionHandler.class);

  /** Converts cloud function request to request model. */
  protected abstract Request toRequest(HttpRequest httpRequest) throws ServiceException;

  /** Processes given request and returns response. */
  protected abstract Response processRequest(Request request) throws ServiceException;

  /** Converts response model to cloud function response. Can be overridden in subclass. */
  // TODO: Provide default implementation that converts response proto to JSON.
  protected abstract void toCloudFunctionResponse(HttpResponse httpResponse, Response response)
      throws IOException;

  @Override
  public void service(HttpRequest httpRequest, HttpResponse httpResponse) throws IOException {
    try {
      toCloudFunctionResponse(httpResponse, processRequest(toRequest(httpRequest)));
    } catch (ServiceException exception) {
      logger.warn("ServiceException occurred during handling, returning error response", exception);
      ErrorResponse response = toErrorResponse(exception);
      createCloudFunctionResponseFromProto(
          httpResponse, response, exception.getErrorCode().getHttpStatusCode(), emptyMap());
    } catch (Exception exception) {
      ServiceException serviceException = ServiceException.ofUnknownException(exception);
      logger.warn("Exception occurred during handling, returning error response", serviceException);
      ErrorResponse response = toErrorResponse(serviceException);
      createCloudFunctionResponseFromProto(
          httpResponse, response, serviceException.getErrorCode().getHttpStatusCode(), emptyMap());
    }
  }
}
