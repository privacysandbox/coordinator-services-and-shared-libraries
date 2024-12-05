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

import com.google.api.client.auth.openidconnect.IdToken;
import com.google.api.client.json.gson.GsonFactory;
import com.google.cloud.functions.HttpFunction;
import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.common.base.Stopwatch;
import com.google.common.collect.ImmutableMap;
import com.google.protobuf.util.Durations;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.api.model.HttpMethod;
import com.google.scp.shared.otel.OTelSemConMetrics;
import io.opentelemetry.api.OpenTelemetry;
import io.opentelemetry.api.common.Attributes;
import io.opentelemetry.api.common.AttributesBuilder;
import io.opentelemetry.api.metrics.DoubleHistogram;
import io.opentelemetry.semconv.HttpAttributes;
import java.io.IOException;
import java.util.List;
import java.util.Map.Entry;
import java.util.Optional;
import java.util.concurrent.TimeUnit;
import java.util.regex.Pattern;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

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

  private static final Logger logger = LoggerFactory.getLogger(CloudFunctionServiceBase.class);

  /** Override below if OpenTelemetry SDK is needed */
  protected OpenTelemetry OTel;

  private final DoubleHistogram httpServerDurationHistogram;

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

  @Override
  public void service(HttpRequest httpRequest, HttpResponse httpResponse) throws Exception {
    HttpResponseWrapper httpResponseWrapper = HttpResponseWrapper.from(httpResponse);
    Stopwatch stopwatch = Stopwatch.createStarted();
    ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>> requestHandlerMap =
        getRequestHandlerMap();
    String requestMethod = httpRequest.getMethod();
    ImmutableMap<Pattern, CloudFunctionRequestHandler> pathToRequestHandlerMap;
    // Check if request method is valid
    if (!HttpMethod.isValid(requestMethod)
        || (pathToRequestHandlerMap = requestHandlerMap.get(HttpMethod.valueOf(requestMethod)))
            == null) {
      handleInvalidHTTPRequest(
          httpResponseWrapper,
          INVALID_ARGUMENT,
          INVALID_HTTP_METHOD.name(),
          String.format("Unsupported HTTP method: '%s'", requestMethod));
      recordOTelMetrics(stopwatch, httpRequest, httpResponseWrapper);
      return;
    }
    Optional<Entry<Pattern, CloudFunctionRequestHandler>> potentialEntry =
        pathToRequestHandlerMap.entrySet().stream()
            .filter(entry -> entry.getKey().matcher(httpRequest.getPath()).matches())
            .findFirst();

    // Check if a potential path entry exist in the map.
    if (potentialEntry.isEmpty()) {
      handleInvalidHTTPRequest(
          httpResponseWrapper,
          NOT_FOUND,
          INVALID_URL_PATH_OR_VARIABLE.name(),
          String.format("Request handler for URL path not found: '%s'", httpRequest.getPath()));
      recordOTelMetrics(stopwatch, httpRequest, httpResponseWrapper);
      return;
    }

    try {
      potentialEntry.get().getValue().handleRequest(httpRequest, httpResponseWrapper);
    } catch (RuntimeException e) {
      // In the case of an unchecked exception, set the status code to 500.
      httpResponseWrapper.setStatusCode(500);
      throw e;
    } finally {
      // Record the OTel metric even if there has been an exception. For checked exception, the
      // requestHandler already sets the status code appropriately, so there is no need to set the
      // status code here.
      recordOTelMetrics(stopwatch, httpRequest, httpResponseWrapper);
    }
  }

  private void recordOTelMetrics(
      Stopwatch stopwatch, HttpRequest httpRequest, HttpResponseWrapper httpResponseWrapper) {
    AttributesBuilder attributesBuilder = Attributes.builder();
    attributesBuilder.put(HttpAttributes.HTTP_REQUEST_METHOD, httpRequest.getMethod());
    Integer statusCode = httpResponseWrapper.getStatusCode();
    if (statusCode != null) {
      attributesBuilder.put(HttpAttributes.HTTP_RESPONSE_STATUS_CODE, statusCode);
    }
    long elapsedMillis = stopwatch.elapsed(TimeUnit.MILLISECONDS);
    double elapsedSeconds = Durations.toSecondsAsDouble(Durations.fromMillis(elapsedMillis));
    List<String> authHeader = httpRequest.getHeaders().get("Authorization");
    if (authHeader != null && !authHeader.isEmpty()) {
      // Get the string for the Authorization header, should be in the format of `Bearer <token>`
      String authString = authHeader.get(0);
      String[] authStringSplit = authString.split(" ");
      if (authStringSplit.length == 2) {
        String bearerToken = authStringSplit[1];
        try {
          // Use IdToken class to parse token. Note that Cloud Run obfuscate the signature by
          // substituting it with SIGNATURE_REMOVED_BY_GOOGLE. This means we can't directly use
          // other JWT libraries.
          IdToken idToken = IdToken.parse(GsonFactory.getDefaultInstance(), bearerToken);
          // Subject field is always present.
          String subject = idToken.getPayload().getSubject();
          // Email field may be missing.
          String email = (String) idToken.getPayload().getUnknownKeys().get("email");
          attributesBuilder.put("client_subject", subject);
          attributesBuilder.put("client_email", email);
        } catch (Exception e) {
          // Throws exception if token cannot be parsed.
          logger.warn(
              "Unable to parse bearer token. Client ID not being recorded by OTel. Token: {}",
              authString);
        }
      } else {
        logger.warn(
            "Bearer token is not in the correct format. Client ID not being recorded by OTel."
                + " Token: {}",
            authString);
      }
    }
    this.httpServerDurationHistogram.record(elapsedSeconds, attributesBuilder.build());
  }

  /** Helper function to return error response for invalid HTTP requests. */
  private void handleInvalidHTTPRequest(
      HttpResponse httpResponse, Code code, String reason, String message) throws IOException {
    ServiceException exception = new ServiceException(code, reason, message);
    ErrorResponse response = toErrorResponse(exception);
    createCloudFunctionResponseFromProto(
        httpResponse, response, exception.getErrorCode().getHttpStatusCode(), allHeaders());
  }
}
