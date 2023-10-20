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

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.common.collect.ImmutableSet;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.URI;
import java.time.Duration;
import org.apache.http.HttpException;
import org.apache.http.HttpRequest;
import org.apache.http.HttpRequestInterceptor;
import org.apache.http.client.config.RequestConfig;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.protocol.HttpContext;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class HttpClientWrapperTest {
  @Test
  public void execute_succeeds() throws Exception {
    HttpClientWrapper httpClient = HttpClientWrapper.builder().build();
    int responseCode = 200;
    TestHandler testHandler = new TestHandler(responseCode);

    HttpClientResponse actualResponse = callServer(httpClient, testHandler);

    assertThat(actualResponse.statusCode()).isEqualTo(responseCode);
  }

  @Test
  public void execute_withRetriableCode_retries() throws Exception {
    int responseCode = 503;
    int maxAttempts = 3;
    HttpClientWrapper httpClient =
        HttpClientWrapper.builder()
            .setMaxAttempt(maxAttempts)
            .setRetryOnStatusCodes(ImmutableSet.of(responseCode))
            .build();
    TestHandler testHandler = new TestHandler(responseCode);

    HttpClientResponse actualResponse = callServer(httpClient, testHandler);

    assertThat(testHandler.calledCount).isEqualTo(maxAttempts);
    assertThat(actualResponse.statusCode()).isEqualTo(responseCode);
  }

  @Test
  public void execute_withNonRetriableCode_noRetries() throws Exception {
    int returnedResponseCode = 403;
    int toBeRetriedCode = 503;
    int maxAttempts = 3;
    HttpClientWrapper httpClient =
        HttpClientWrapper.builder()
            .setMaxAttempt(maxAttempts)
            .setRetryOnStatusCodes(ImmutableSet.of(toBeRetriedCode))
            .build();
    TestHandler testHandler = new TestHandler(returnedResponseCode);

    HttpClientResponse actualResponse = callServer(httpClient, testHandler);

    assertThat(testHandler.calledCount).isEqualTo(1);
    assertThat(actualResponse.statusCode()).isEqualTo(returnedResponseCode);
  }

  @Test
  public void execute_withRetriableException_retries() {
    int maxAttempts = 4;
    HttpClientWrapper httpClient =
        HttpClientWrapper.builder()
            .setMaxAttempt(maxAttempts)
            .setRetryOnExceptions(ImmutableSet.of(IOException.class))
            .build();
    TestHandler testHandler = new TestHandler();
    testHandler.setToThrowException(true);

    assertThrows(IOException.class, () -> callServer(httpClient, testHandler));
    assertThat(testHandler.calledCount).isEqualTo(maxAttempts);
  }

  @Test
  public void execute_withNonRetriableException_noRetries() {
    int maxAttempts = 3;
    HttpClientWrapper httpClient =
        HttpClientWrapper.builder()
            .setMaxAttempt(maxAttempts)
            .setRetryOnExceptions(ImmutableSet.of(IllegalStateException.class))
            .build();
    TestHandler testHandler = new TestHandler();
    testHandler.setToThrowException(true);

    assertThrows(IOException.class, () -> callServer(httpClient, testHandler));
    assertThat(testHandler.calledCount).isEqualTo(1);
  }

  @Test
  public void execute_retriesUntilSuccess() throws Exception {
    int statusCodeToRetry = 500;
    int maxAttempts = 5;
    HttpClientWrapper httpClient =
        HttpClientWrapper.builder()
            .setMaxAttempt(maxAttempts)
            .setRetryOnExceptions(ImmutableSet.of(IOException.class))
            .setRetryOnStatusCodes(ImmutableSet.of(statusCodeToRetry))
            .build();
    TestHandler testHandler = new TestHandler(statusCodeToRetry, -1, -1, 200, statusCodeToRetry);
    testHandler.setToThrowException(false, true, true, false, true);

    HttpClientResponse response = callServer(httpClient, testHandler);

    assertThat(response.statusCode()).isEqualTo(200);
    assertThat(testHandler.calledCount).isEqualTo(4);
  }

  @Test
  public void execute_withExponentialBackOff() throws Exception {
    int[] returnCodes = {500, 503, 429, 504, 502};
    int maxAttempts = 5;
    HttpClientWrapper httpClient =
        HttpClientWrapper.builder()
            .setRetryOnStatusCodes(ImmutableSet.of(500, 503, 429, 504, 502))
            .setExponentialBackoff(Duration.ofMillis(1000), 1.001, maxAttempts)
            .build();
    TestHandler testHandler = new TestHandler(returnCodes);

    HttpClientResponse response = callServer(httpClient, testHandler);

    assertThat(response.statusCode()).isEqualTo(502);
    assertThat(testHandler.calledCount).isEqualTo(maxAttempts);
  }

  @Test
  public void execute_withInterceptor() throws Exception {
    TestInterceptor interceptor = new TestInterceptor();
    HttpClientWrapper httpClient = HttpClientWrapper.builder().setInterceptor(interceptor).build();
    int responseCode = 200;
    TestHandler testHandler = new TestHandler(responseCode);

    callServer(httpClient, testHandler);

    assertThat(interceptor.interceptorCallCount).isEqualTo(1);
  }

  @Test
  public void execute_withInterceptor_withRetriableCode_interceptorCalledWithRetry()
      throws Exception {
    int returnCode = 503;
    TestInterceptor interceptor = new TestInterceptor();
    HttpClientWrapper httpClient =
        HttpClientWrapper.builder()
            .setRetryOnStatusCodes(ImmutableSet.of(returnCode))
            .setInterceptor(interceptor)
            .build();
    TestHandler testHandler = new TestHandler(returnCode);

    callServer(httpClient, testHandler);

    assertThat(interceptor.interceptorCallCount).isEqualTo(6);
  }

  @Test
  public void execute_withDefaultConfig_withRetriableCode_retries() throws Exception {
    HttpClientWrapper httpClient = HttpClientWrapper.createDefault();
    TestHandler testHandler = new TestHandler(503);

    HttpClientResponse response = callServer(httpClient, testHandler);

    assertThat(response.statusCode()).isEqualTo(503);
    assertThat(testHandler.calledCount).isEqualTo(6);
  }

  private HttpClientResponse callServer(HttpClientWrapper client, TestHandler testHandler)
      throws Exception {
    HttpServer server = provideServer(testHandler);
    Thread thread = new Thread(server::start);
    thread.start();
    URI uri =
        URI.create(
            "http://"
                + server.getAddress().getAddress().getLoopbackAddress().getHostName()
                + ":"
                + server.getAddress().getPort()
                + "/test");
    HttpGet request = new HttpGet(uri);
    request.setConfig(
        RequestConfig.custom()
            .setConnectionRequestTimeout(1000)
            .setConnectTimeout(1000)
            .setSocketTimeout(1000)
            .build());
    HttpClientResponse response = client.execute(request);
    server.stop(0);
    thread.join();
    return response;
  }

  private HttpServer provideServer(TestHandler testHandler) throws Exception {
    // Picks a random available port.
    HttpServer server = HttpServer.create(new InetSocketAddress(0), 0);
    server.createContext("/test", testHandler);
    server.setExecutor(null); // creates a default executor
    return server;
  }

  /** A test interceptor to be called before http request is made by the client. */
  private static class TestInterceptor implements HttpRequestInterceptor {
    private int interceptorCallCount = 0;

    @Override
    public void process(HttpRequest var1, HttpContext var2) throws HttpException, IOException {
      interceptorCallCount++;
    }
  }

  /** A test handler to send the chosen status code when request is processed. */
  private static class TestHandler implements HttpHandler {
    private int calledCount = 0;
    private final int[] statusCodes;
    private boolean[] throwExceptions;

    TestHandler(int... statusCode) {
      this.statusCodes = statusCode;
      throwExceptions = new boolean[] {false};
    }

    private void setToThrowException(boolean... throwExceptions) {
      this.throwExceptions = throwExceptions;
    }

    @Override
    public void handle(HttpExchange t) throws IOException {
      calledCount++;
      boolean toThrow =
          calledCount < throwExceptions.length
              ? throwExceptions[calledCount - 1]
              : throwExceptions[throwExceptions.length - 1];
      if (toThrow) {
        throw new IOException();
      }
      int statusCode =
          calledCount < statusCodes.length
              ? statusCodes[calledCount - 1]
              : statusCodes[statusCodes.length - 1];
      t.sendResponseHeaders(statusCode, 0);
      OutputStream responseBody = t.getResponseBody();
      responseBody.write(1);
      responseBody.close();
    }
  }
}
