/*
 * Copyright 2023 Google LLC
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

package com.google.scp.shared.clients;

import org.apache.http.client.ServiceUnavailableRetryStrategy;

/** Class for default http client retry strategy * */
public class DefaultHttpClientRetryStrategy implements ServiceUnavailableRetryStrategy {
  public static final int HTTP_CLIENT_MAX_RETRY_COUNT = 5;
  private static final DefaultHttpClientRetryStrategy INSTANCE =
      new DefaultHttpClientRetryStrategy();

  private DefaultHttpClientRetryStrategy() {}

  public static DefaultHttpClientRetryStrategy getInstance() {
    return INSTANCE;
  }

  @Override
  public boolean retryRequest(
      org.apache.http.HttpResponse response,
      int executionCount,
      org.apache.http.protocol.HttpContext context) {
    int statusCode = response.getStatusLine().getStatusCode();
    // Only retry on 5xx status codes.
    if (statusCode < 500 || statusCode > 599) {
      return false;
    }
    return executionCount < HTTP_CLIENT_MAX_RETRY_COUNT;
  }

  @Override
  public long getRetryInterval() {
    // Retries after 2000 milliseconds.
    return 2000;
  }
}
