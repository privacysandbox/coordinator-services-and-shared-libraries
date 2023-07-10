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

package com.google.scp.shared.api.util;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.ImmutableSet;
import com.google.inject.Inject;
import io.github.resilience4j.core.IntervalFunction;
import io.github.resilience4j.retry.Retry;
import io.github.resilience4j.retry.RetryConfig;
import io.github.resilience4j.retry.RetryRegistry;
import java.io.IOException;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Map;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import org.apache.hc.client5.http.async.methods.SimpleHttpRequest;
import org.apache.hc.client5.http.async.methods.SimpleHttpResponse;
import org.apache.hc.client5.http.config.RequestConfig;
import org.apache.hc.client5.http.impl.async.CloseableHttpAsyncClient;
import org.apache.hc.client5.http.impl.async.HttpAsyncClients;
import org.apache.hc.core5.http.ContentType;
import org.apache.hc.core5.http.Header;
import org.apache.hc.core5.http.HttpRequestInterceptor;
import org.apache.hc.core5.http.HttpStatus;
import org.apache.hc.core5.http.Method;
import org.apache.hc.core5.reactor.IOReactorStatus;
import org.apache.hc.core5.util.Timeout;
import software.amazon.awssdk.services.sts.model.StsException;

/**
 * This is a wrapper on top the bare-bones Apache HttpClient 5.0. Every request executed by the
 * HttpClient in this wrapper is subject to be intercepted by an HttpRequestInterceptor.
 */
public class HttpClientWithInterceptor {

  private static final Duration INITIAL_BACKOFF_INTERVAL = Duration.ofSeconds(2);
  private static final Duration MAX_BACKOFF_INTERVAL = Duration.ofSeconds(30);
  private static final int BACKOFF_MULTIPLIER = 2;
  private static final int MAX_ATTEMPTS = 6;

  private static final ImmutableSet<Integer> RETRYABLE_HTTP_STATUS_CODES =
      ImmutableSet.of(
          HttpStatus.SC_REQUEST_TIMEOUT, // 408
          HttpStatus.SC_TOO_MANY_REQUESTS, // 429
          HttpStatus.SC_INTERNAL_SERVER_ERROR, // 500
          HttpStatus.SC_BAD_GATEWAY, // 502
          HttpStatus.SC_SERVICE_UNAVAILABLE, // 503
          HttpStatus.SC_GATEWAY_TIMEOUT // 504
          );

  @VisibleForTesting
  static final RetryRegistry RETRY_REGISTRY =
      RetryRegistry.of(
          RetryConfig.<SimpleHttpResponse>custom()
              .maxAttempts(MAX_ATTEMPTS)
              /*
               * Retries 5 times (in addition to initial attempt) with initial interval of 2s between calls.
               * Backoff interval increases exponentially between retries viz. 2s, 4s, 8s, 16s, 30s respectively
               */
              .intervalFunction(
                  IntervalFunction.ofExponentialBackoff(
                      INITIAL_BACKOFF_INTERVAL, BACKOFF_MULTIPLIER, MAX_BACKOFF_INTERVAL))
              .retryOnException(HttpClientWithInterceptor::retryOnExceptionPredicate)
              .retryOnResult(response -> RETRYABLE_HTTP_STATUS_CODES.contains(response.getCode()))
              .build());

  private static final long REQUEST_TIMEOUT_DURATION = Duration.ofSeconds(60).toMillis();
  private static final RequestConfig REQUEST_CONFIG =
      RequestConfig.custom()
          // Timeout for requesting a connection from internal connection manager
          .setConnectionRequestTimeout(Timeout.ofMilliseconds(REQUEST_TIMEOUT_DURATION))
          // Timeout for establishing a request to host
          .setConnectTimeout(Timeout.ofMilliseconds(REQUEST_TIMEOUT_DURATION))
          // Timeout for waiting to get a response from the server
          .setResponseTimeout(Timeout.ofMilliseconds(REQUEST_TIMEOUT_DURATION))
          .build();

  private static boolean retryOnExceptionPredicate(Throwable e) {
    // If the HTTP error code is UNAUTHORIZED or FORBIDDEN in StsException, this
    // likely means that the client is not able to get auth token from AWS STS.
    // Don't retry in this case.
    if (e instanceof StsException) {
      StsException stsException = (StsException) e;
      return stsException.statusCode() != HttpStatus.SC_UNAUTHORIZED
          && stsException.statusCode() != HttpStatus.SC_FORBIDDEN;
    }

    // Otherwise, retry
    return true;
  }

  private final CloseableHttpAsyncClient httpClient;
  private final Retry retryConfig;

  @Inject
  public HttpClientWithInterceptor(HttpRequestInterceptor authTokenInterceptor) {
    this.httpClient =
        HttpAsyncClients.customHttp2().addRequestInterceptorFirst(authTokenInterceptor).build();
    if (!IOReactorStatus.ACTIVE.equals(httpClient.getStatus())) {
      httpClient.start();
    }
    this.retryConfig = RETRY_REGISTRY.retry("awsHttpClientRetryConfig");
  }

  /**
   * This constructor is reserved specifically for unit tests pertaining to this class. This
   * constructor will not setup the request interceptor in the httpclient, thus rendering the object
   * useless for actual client use-cases outside of unit testing. It also accepts a retryregistry
   * that allows for quicker testing. As of now the actual retry delay is in order of several
   * seconds which would make the unit tests extremely slow
   *
   * @param httpClient
   */
  @VisibleForTesting
  HttpClientWithInterceptor(CloseableHttpAsyncClient httpClient, RetryRegistry retryRegistry) {
    this.httpClient = httpClient;
    if (!IOReactorStatus.ACTIVE.equals(httpClient.getStatus())) {
      httpClient.start();
    }
    this.retryConfig = retryRegistry.retry("awsHttpClientRetryConfig");
  }

  /**
   * @param endpointUrl - complete request url (authority + relative path) Example:
   *     https://client.example.com/v1/getExamples
   * @param headers - Any headers that need to be added to the request
   * @throws IOException in case of any issues executing the HTTP request. Example includes but is
   *     not limited to network connection issues
   */
  public HttpClientResponse executeGet(String endpointUrl, Map<String, String> headers)
      throws IOException {
    URI endpointUri = URI.create(endpointUrl);

    SimpleHttpRequest httpRequest = SimpleHttpRequest.create(Method.GET, endpointUri);
    httpRequest.setConfig(REQUEST_CONFIG);
    headers.forEach(httpRequest::setHeader);
    SimpleHttpResponse response = executeRequest(httpRequest);
    return HttpClientResponse.create(
        response.getCode(), response.getBodyText(), parseResponseHeaders(response.getHeaders()));
  }

  /**
   * @param endpointUrl - complete request url (authority + relative path). Example:
   *     https://client.example.com/v1/getExamples
   * @param payload - Request Body. The body needs to be of type application/json
   * @param headers - Any headers that need to be added to the request
   * @throws IOException in case of any issues executing the HTTP request. Example includes but is
   *     not limited to network connection issues
   */
  public HttpClientResponse executePost(
      String endpointUrl, String payload, Map<String, String> headers) throws IOException {
    URI endpointUri = URI.create(endpointUrl);

    SimpleHttpRequest httpRequest = SimpleHttpRequest.create(Method.POST, endpointUri);
    httpRequest.setConfig(REQUEST_CONFIG);
    headers.forEach(httpRequest::setHeader);
    httpRequest.setHeader("content-length", payload.getBytes(StandardCharsets.UTF_8).length);
    httpRequest.setBody(payload, ContentType.APPLICATION_JSON);
    SimpleHttpResponse response = executeRequest(httpRequest);
    return HttpClientResponse.create(
        response.getCode(), response.getBodyText(), parseResponseHeaders(response.getHeaders()));
  }

  private SimpleHttpResponse executeRequest(SimpleHttpRequest httpRequest) throws IOException {
    try {
      return Retry.decorateCheckedSupplier(
              retryConfig, () -> httpClient.execute(httpRequest, null).get())
          .apply();
    } catch (StsException e) {
      // The HTTP request sent by HttpClientWithInterceptor can potentially be intercepted by
      // other HTTP interceptor, such as AwsAuthTokenInterceptor. These interceptors can
      // potentially throws some RuntimeException. StsException is a RuntimeException
      // that might be thrown by AwsAuthTokenInterceptor when the interceptor is not able to get
      // auth token (e.g. when the client can't assume another AWS role).

      // If the HTTP error code is UNAUTHORIZED or FORBIDDEN, don't throw IOException so that
      // PrivacyBudgetClientImpl and TransactionEngineImpl do not retry the HTTP request. These
      // error codes are generally not retryable within a short period of time.
      String errorMessage =
          e.awsErrorDetails() != null
              ? e.awsErrorDetails().errorMessage()
              : "Unknown error message";
      if (e.statusCode() == HttpStatus.SC_UNAUTHORIZED) {
        return SimpleHttpResponse.create(HttpStatus.SC_UNAUTHORIZED, errorMessage);
      } else if (e.statusCode() == HttpStatus.SC_FORBIDDEN) {
        return SimpleHttpResponse.create(HttpStatus.SC_FORBIDDEN, errorMessage);
      }

      // Other exception will be rethrown as IOException
      throw new IOException(e);
    } catch (Throwable e) {
      /**
       * We wrap all exceptions coming out of this operation into an IOException because we believe
       * we should retry the transaction phase in such situations. PrivacyBudgetClientImpl and
       * TransactionEngineImpl rely on IOException being thrown to decide if the phase should be
       * retried.
       */
      throw new IOException(e);
    }
  }

  private static Map<String, String> parseResponseHeaders(Header... headers) {
    return Stream.of(headers).collect(Collectors.toMap(Header::getName, Header::getValue));
  }
}
