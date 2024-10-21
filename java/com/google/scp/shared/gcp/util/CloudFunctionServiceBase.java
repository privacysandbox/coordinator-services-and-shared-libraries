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
import com.google.common.base.Stopwatch;
import com.google.common.collect.ImmutableMap;
import com.google.protobuf.util.Durations;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.HttpMethod;
import com.google.scp.shared.otel.OTelSemConMetrics;
import io.opentelemetry.api.OpenTelemetry;
import io.opentelemetry.api.common.Attributes;
import io.opentelemetry.api.common.AttributesBuilder;
import io.opentelemetry.api.metrics.DoubleHistogram;
import io.opentelemetry.semconv.HttpAttributes;
import java.util.Map.Entry;
import java.util.Optional;
import java.util.concurrent.TimeUnit;
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

  /** Override below if OpenTelemetry SDK is needed */
  protected OpenTelemetry OTel;

  private DoubleHistogram httpServerDurationHistogram;

  protected final ImmutableMap<String, String> allHeaders() {
    return new ImmutableMap.Builder<String, String>()
        .putAll(customHeaders())
        .put("content-type", "application/json") // enforce json content-type
        .build();
  }

  /** Use this constructor if OpenTelemetry SDK is not needed */
  protected CloudFunctionServiceBase() {
    this(OpenTelemetry.noop());
  }

  /** Use this constructor if OpenTelemetry SDK is needed */
  protected CloudFunctionServiceBase(OpenTelemetry OTel) {
    this.OTel = OTel;
    this.httpServerDurationHistogram =
        OTel.getMeter(this.getClass().getName())
            .histogramBuilder(OTelSemConMetrics.HTTP_SERVER_REQUEST_DURATION)
            .setExplicitBucketBoundariesAdvice(OTelSemConMetrics.LATENCY_BUCKETS)
            .build();
  }

  protected OpenTelemetry getOTel() {
    return this.OTel;
  }

  @Override
  public void service(HttpRequest httpRequest, HttpResponse httpResponse) throws Exception {
    Stopwatch stopwatch = Stopwatch.createStarted();
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
    long elapsedMillis = stopwatch.elapsed(TimeUnit.MILLISECONDS);
    double elapsedSeconds = Durations.toSecondsAsDouble(Durations.fromMillis(elapsedMillis));
    AttributesBuilder attributesBuilder = Attributes.builder();
    attributesBuilder.put(HttpAttributes.HTTP_ROUTE, requestMethod);
    this.httpServerDurationHistogram.record(elapsedSeconds, attributesBuilder.build());
  }
}
