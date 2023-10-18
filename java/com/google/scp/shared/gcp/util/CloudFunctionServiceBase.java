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

import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_HTTP_METHOD;
import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_URL_PATH_OR_VARIABLE;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static com.google.scp.shared.api.util.ErrorUtil.toErrorResponse;
import static com.google.scp.shared.gcp.util.CloudFunctionUtil.createCloudFunctionResponseFromProto;

import com.google.cloud.functions.HttpFunction;
import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.common.collect.ImmutableMap;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.HttpMethod;
import java.util.Map.Entry;
import java.util.Optional;
import java.util.regex.Pattern;

/**
 * Acts as a base class for service implemented as Cloud Function. Maps given http request to
 * Request Handler.
 */
public abstract class CloudFunctionServiceBase implements HttpFunction {

  protected abstract ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
      getRequestHandlerMap();

  /** Override the below if subclass needs to add custom header */
  protected ImmutableMap<String, String> customHeaders() {
    return ImmutableMap.of();
  }

  protected final ImmutableMap<String, String> allHeaders() {
    return new ImmutableMap.Builder<String, String>()
        .putAll(customHeaders())
        .put("content-type", "application/json") // enforce json content-type
        .build();
  }

  @Override
  public void service(HttpRequest httpRequest, HttpResponse httpResponse) throws Exception {
    ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>> requestHandlerMap =
        getRequestHandlerMap();

    String requestMethod = httpRequest.getMethod();
    if (!HttpMethod.isValid(requestMethod)
        || !requestHandlerMap.containsKey(HttpMethod.valueOf(requestMethod))) {
      ServiceException exception =
          new ServiceException(
              /* code= */ INVALID_ARGUMENT,
              /* reason= */ INVALID_HTTP_METHOD.name(),
              /* message= */ String.format("Unsupported http method: '%s'", requestMethod));
      ErrorResponse response = toErrorResponse(exception);
      createCloudFunctionResponseFromProto(
          httpResponse, response, exception.getErrorCode().getHttpStatusCode(), allHeaders());
    } else {
      HttpMethod httpMethod = HttpMethod.valueOf(requestMethod);
      ImmutableMap<Pattern, CloudFunctionRequestHandler> pathToRequestHandlerMap =
          requestHandlerMap.get(httpMethod);
      Optional<Entry<Pattern, CloudFunctionRequestHandler>> potentialEntry =
          pathToRequestHandlerMap.entrySet().stream()
              .filter(entry -> entry.getKey().matcher(httpRequest.getPath()).matches())
              .findFirst();
      if (potentialEntry.isEmpty()) {
        ServiceException exception =
            new ServiceException(
                /* code= */ NOT_FOUND,
                /* reason= */ INVALID_URL_PATH_OR_VARIABLE.name(),
                /* message= */ String.format("Unsupported URL path: '%s'", httpRequest.getPath()));
        ErrorResponse response = toErrorResponse(exception);
        createCloudFunctionResponseFromProto(
            httpResponse, response, exception.getErrorCode().getHttpStatusCode(), allHeaders());
      } else {
        potentialEntry.get().getValue().handleRequest(httpRequest, httpResponse);
      }
    }
  }
}
