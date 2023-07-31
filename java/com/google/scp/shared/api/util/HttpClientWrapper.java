/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.scp.shared.api.util;

import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;
import io.github.resilience4j.core.IntervalFunction;
import io.github.resilience4j.retry.Retry;
import io.github.resilience4j.retry.RetryConfig;
import io.github.resilience4j.retry.RetryRegistry;
import java.io.IOException;
import java.time.Duration;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Stream;
import org.apache.http.Header;
import org.apache.http.HttpRequestInterceptor;
import org.apache.http.HttpStatus;
import org.apache.http.client.methods.CloseableHttpResponse;
import org.apache.http.client.methods.HttpRequestBase;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClientBuilder;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.util.EntityUtils;

/** Wrapper class to create HttpClient with interceptor, supporting exponential retry strategy. */
public class HttpClientWrapper {

  private static final int DEFAULT_INTERVAL_MILLIS = 2000;

  private static final int DEFAULT_MAX_ATTEMPTS = 6;

  private static final IntervalFunction DEFAULT_INTERVAL_FUNCTION =
      numOfAttempts -> (long) DEFAULT_INTERVAL_MILLIS;

  private static final ImmutableSet<Class<? extends Throwable>> DEFAULT_EXCEPTIONS_TO_RETRY_ON =
      ImmutableSet.of(IOException.class);

  private static final ImmutableSet<Integer> DEFAULT_STATUS_CODES_TO_RETRY_ON =
      ImmutableSet.of(
          HttpStatus.SC_REQUEST_TIMEOUT, // 408
          HttpStatus.SC_TOO_MANY_REQUESTS, // 429
          HttpStatus.SC_INTERNAL_SERVER_ERROR, // 500
          HttpStatus.SC_BAD_GATEWAY, // 502
          HttpStatus.SC_SERVICE_UNAVAILABLE, // 503
          HttpStatus.SC_GATEWAY_TIMEOUT // 504
          );

  private final CloseableHttpClient httpClient;

  private final Retry retryConfig;

  /** Returns a builder for this class. */
  public static HttpClientWrapper.Builder builder() {
    return new HttpClientWrapper.Builder();
  }

  /** Creates a default instance for this class with default retry strategies and no interceptor. */
  public static HttpClientWrapper createDefault() {
    return new HttpClientWrapper.Builder().build();
  }

  private HttpClientWrapper(CloseableHttpClient httpClient, Retry retryConfig) {
    this.httpClient = httpClient;
    this.retryConfig = retryConfig;
  }

  /**
   * Executes the request, applying the interceptor and the specified retry strategy.
   *
   * @param request http request
   * @throws IOException
   */
  public <T extends HttpRequestBase> HttpClientResponse execute(T request) throws IOException {
    try {
      return Retry.decorateCheckedSupplier(retryConfig, () -> executeRequest(request)).apply();
    } catch (Throwable throwable) {
      throw new IOException(throwable);
    }
  }

  /**
   * Executes the request and reads the response into HttpClientResponse. Reading the response is
   * necessary before retrying to prevent connection leak.
   *
   * @param request Http request
   * @throws IOException
   */
  private <T extends HttpRequestBase> HttpClientResponse executeRequest(T request)
      throws IOException {
    CloseableHttpResponse response = httpClient.execute(request);
    return HttpClientResponse.create(
        response.getStatusLine().getStatusCode(),
        EntityUtils.toString(response.getEntity()),
        Stream.of(response.getAllHeaders())
            .collect(ImmutableMap.toImmutableMap(Header::getName, Header::getValue)));
  }

  private static HttpClientWrapper createHttpClient(
      Optional<HttpRequestInterceptor> interceptor,
      Optional<IntervalFunction> intervalFunction,
      Optional<Integer> maxAttempts,
      Optional<ImmutableSet<Class<? extends Throwable>>> retryOnExceptions,
      Optional<Set<Integer>> retryOnStatusCodes) {
    HttpClientBuilder httpClientBuilder =
        HttpClients.custom().disableAutomaticRetries(); // Retries are handled separately.
    interceptor.ifPresent(
        httpInterceptor -> httpClientBuilder.addInterceptorFirst(httpInterceptor));
    RetryConfig retryConfig =
        buildRetryConfig(intervalFunction, maxAttempts, retryOnExceptions, retryOnStatusCodes);

    return new HttpClientWrapper(
        httpClientBuilder.build(), RetryRegistry.of(retryConfig).retry("httpClient"));
  }

  private static RetryConfig buildRetryConfig(
      Optional<IntervalFunction> intervalFunction,
      Optional<Integer> maxAttempts,
      Optional<ImmutableSet<Class<? extends Throwable>>> retryOnExceptions,
      Optional<Set<Integer>> retryOnStatusCodes) {
    RetryConfig.Builder<HttpClientResponse> retryConfigBuilder = RetryConfig.custom();
    retryConfigBuilder.intervalFunction(intervalFunction.orElse(DEFAULT_INTERVAL_FUNCTION));
    retryConfigBuilder.maxAttempts(maxAttempts.orElse(DEFAULT_MAX_ATTEMPTS));
    ImmutableSet<Class<? extends Throwable>> exceptionsToRetryOn =
        retryOnExceptions.orElse(DEFAULT_EXCEPTIONS_TO_RETRY_ON);
    retryConfigBuilder.retryExceptions(
        exceptionsToRetryOn.toArray(new Class[exceptionsToRetryOn.size()]));
    Set<Integer> statusCodesToRetryOn = retryOnStatusCodes.orElse(DEFAULT_STATUS_CODES_TO_RETRY_ON);
    retryConfigBuilder.retryOnResult(
        response -> statusCodesToRetryOn.contains(response.statusCode()));
    return retryConfigBuilder.build();
  }

  /** Builder class for {@link HttpClientWrapper} */
  public static class Builder {

    private HttpRequestInterceptor interceptor;

    private IntervalFunction intervalFunction;

    private int maxAttempts = DEFAULT_MAX_ATTEMPTS;

    private ImmutableSet<Class<? extends Throwable>> retryExceptions;

    private ImmutableSet<Integer> retryStatusCodes;

    /** Sets the interceptor that should be called before every request. */
    public Builder setInterceptor(HttpRequestInterceptor interceptor) {
      this.interceptor = interceptor;
      return this;
    }

    /** Sets uniform backoff interval. */
    public Builder setInterval(Duration interval) {
      this.intervalFunction = IntervalFunction.of(interval.toMillis());
      return this;
    }

    /**
     * Sets the maximum attempts for the request. This number includes the original call as well;
     * maxAttempts = 3 means 1 call + 2 retries.
     */
    public Builder setMaxAttempt(int maxAttempts) {
      this.maxAttempts = maxAttempts;
      return this;
    }

    /** Sets the exceptions to retry on. */
    public Builder setRetryOnExceptions(ImmutableSet<Class<? extends Throwable>> retryExceptions) {
      this.retryExceptions = retryExceptions;
      return this;
    }

    /** Sets the {@link HttpStatus} codes to retry on. */
    public Builder setRetryOnStatusCodes(ImmutableSet<Integer> retryStatusCodes) {
      this.retryStatusCodes = retryStatusCodes;
      return this;
    }

    /**
     * Sets exponential backoff intervals.
     *
     * @param initialInterval initial interval before the first retry
     * @param multiplier >= 1. the increase in interval after the first retry.
     * @param maxAttempts 1 + retry attempts to make
     */
    public Builder setExponentialBackoff(
        Duration initialInterval, double multiplier, int maxAttempts) {
      this.intervalFunction = IntervalFunction.ofExponentialBackoff(initialInterval, multiplier);
      this.maxAttempts = maxAttempts;
      return this;
    }

    public HttpClientWrapper build() {
      return HttpClientWrapper.createHttpClient(
          Optional.ofNullable(interceptor),
          Optional.ofNullable(intervalFunction),
          Optional.ofNullable(maxAttempts),
          Optional.ofNullable(retryExceptions),
          Optional.ofNullable(retryStatusCodes));
    }
  }
}
