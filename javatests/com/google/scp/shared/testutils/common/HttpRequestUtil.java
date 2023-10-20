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

package com.google.scp.shared.testutils.common;

import io.github.resilience4j.core.IntervalFunction;
import io.github.resilience4j.retry.Retry;
import io.github.resilience4j.retry.RetryConfig;
import io.github.resilience4j.retry.RetryRegistry;
import java.io.IOException;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;

/** Util class with helper methods for HTTP requests. Useful for integration tests to an API. */
public class HttpRequestUtil {

  private HttpRequestUtil() {}

  /** Makes multiple attempts to execute the HTTP request. */
  public static HttpResponse<String> executeRequestWithRetry(
      HttpClient client, HttpRequest request) {
    RetryRegistry registry =
        RetryRegistry.of(
            RetryConfig.<HttpResponse<?>>custom()
                .maxAttempts(8)
                .failAfterMaxAttempts(true)
                .intervalFunction(IntervalFunction.ofExponentialBackoff(200, 2))
                .retryExceptions(IOException.class, IllegalStateException.class)
                .retryOnResult(response -> response.statusCode() == 500)
                .build());
    Retry retry = registry.retry("request");

    try {
      return Retry.decorateCheckedSupplier(
              retry,
              () ->
                  client.send(request, HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8)))
          .apply();
    } catch (Throwable t) {
      throw new RuntimeException("Error with retrying HTTP request", t);
    }
  }
}
