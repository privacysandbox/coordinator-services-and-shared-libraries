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

package com.google.scp.shared.clients;

import static com.google.common.truth.Truth.assertThat;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.URI;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.HttpClients;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class DefaultHttpClientRetryStrategyTest {
  private TestHandler testHandler;

  @Test
  public void retryForStatusCode500() throws Exception {
    HttpServer server = provideServer(500);

    callServer(server);

    assertThat(testHandler.calledCount)
        .isEqualTo(DefaultHttpClientRetryStrategy.HTTP_CLIENT_MAX_RETRY_COUNT);
  }

  @Test
  public void noRetryForStatusCode400() throws Exception {
    HttpServer server = provideServer(400);

    callServer(server);

    assertThat(testHandler.calledCount).isEqualTo(1);
  }

  private void callServer(HttpServer server) throws Exception {
    new Thread(server::start).start();
    HttpClient client = provideClient();
    var uri =
        URI.create(
            "http://"
                + server.getAddress().getAddress().getLoopbackAddress().getHostName()
                + ":"
                + server.getAddress().getPort()
                + "/test");
    client.execute(new HttpGet(uri));
    server.stop(0);
  }

  private HttpServer provideServer(int responseStatusCode) throws Exception {
    // Picks a random available port.
    HttpServer server = HttpServer.create(new InetSocketAddress(0), 0);
    testHandler = new TestHandler(responseStatusCode);
    server.createContext("/test", testHandler);
    server.setExecutor(null); // creates a default executor
    return server;
  }

  private HttpClient provideClient() {
    return HttpClients.custom()
        .setServiceUnavailableRetryStrategy(DefaultHttpClientRetryStrategy.getInstance())
        .build();
  }

  private class TestHandler implements HttpHandler {
    private int calledCount = 0;
    private int statusCode = 0;

    TestHandler(int statusCode) {
      this.statusCode = statusCode;
    }

    @Override
    public void handle(HttpExchange t) throws IOException {
      calledCount++;
      t.sendResponseHeaders(statusCode, 1);
    }
  }
}
