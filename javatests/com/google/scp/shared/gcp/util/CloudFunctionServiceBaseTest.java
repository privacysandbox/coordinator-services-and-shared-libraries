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

import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.common.collect.ImmutableMap;
import com.google.scp.shared.api.model.HttpMethod;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.StringWriter;
import java.util.regex.Pattern;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class CloudFunctionServiceBaseTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Mock private CloudFunctionRequestHandler handler1;
  @Mock private CloudFunctionRequestHandler handler2;
  @Mock private HttpRequest httpRequest;
  @Mock private HttpResponse httpResponse;
  private static String PATH1 = "/v1/createJob";
  private static String PATH2 = "/v1/getJob";
  private static String PATH3 = "/v1/deleteJob";

  @Before
  public void setUp() throws IOException {
    StringWriter httpResponseOut = new StringWriter();
    BufferedWriter writerOut = new BufferedWriter(httpResponseOut);
    lenient().when(httpResponse.getWriter()).thenReturn(writerOut);
  }

  @Test
  public void service_success() throws Exception {
    when(httpRequest.getMethod()).thenReturn(HttpMethod.GET.name());
    when(httpRequest.getPath()).thenReturn(PATH2);

    ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>> requestHandlers =
        ImmutableMap.of(
            HttpMethod.GET,
            ImmutableMap.of(Pattern.compile(PATH2), handler1),
            HttpMethod.POST,
            ImmutableMap.of(Pattern.compile(PATH1), handler2));
    TestService service = new TestService(requestHandlers);
    service.service(httpRequest, httpResponse);

    verify(handler1, times(1)).handleRequest(any(), any());
    verify(handler2, never()).handleRequest(any(), any());
  }

  @Test
  public void service_invalidMethodFails() throws Exception {
    when(httpRequest.getMethod()).thenReturn(HttpMethod.PUT.name());

    ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>> requestHandlers =
        ImmutableMap.of(
            HttpMethod.GET,
            ImmutableMap.of(Pattern.compile(PATH2), handler1),
            HttpMethod.POST,
            ImmutableMap.of(Pattern.compile(PATH1), handler2));
    TestService service = new TestService(requestHandlers);
    service.service(httpRequest, httpResponse);

    verify(httpResponse).setStatusCode(eq(INVALID_ARGUMENT.getHttpStatusCode()));
    verify(handler1, never()).handleRequest(any(), any());
    verify(handler2, never()).handleRequest(any(), any());
  }

  @Test
  public void service_unsupportedMethodFails() throws Exception {
    when(httpRequest.getMethod()).thenReturn("FAKE");

    ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>> requestHandlers =
        ImmutableMap.of(
            HttpMethod.GET,
            ImmutableMap.of(Pattern.compile(PATH2), handler1),
            HttpMethod.POST,
            ImmutableMap.of(Pattern.compile(PATH1), handler2));
    TestService service = new TestService(requestHandlers);
    service.service(httpRequest, httpResponse);

    verify(httpResponse).setStatusCode(eq(INVALID_ARGUMENT.getHttpStatusCode()));
    verify(handler1, never()).handleRequest(any(), any());
    verify(handler2, never()).handleRequest(any(), any());
  }

  @Test
  public void service_invalidPathFails() throws Exception {
    when(httpRequest.getMethod()).thenReturn(HttpMethod.GET.name());
    when(httpRequest.getPath()).thenReturn(PATH3);

    ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>> requestHandlers =
        ImmutableMap.of(
            HttpMethod.GET,
            ImmutableMap.of(Pattern.compile(PATH2), handler1),
            HttpMethod.POST,
            ImmutableMap.of(Pattern.compile(PATH1), handler2));
    TestService service = new TestService(requestHandlers);
    service.service(httpRequest, httpResponse);

    verify(httpResponse).setStatusCode(eq(NOT_FOUND.getHttpStatusCode()));
    verify(handler1, never()).handleRequest(any(), any());
    verify(handler2, never()).handleRequest(any(), any());
  }

  @Test
  public void service_trailingCharactersFails() throws Exception {
    String goodPath = "/v1/getJob";
    String trailingPath = "/v1/getJobbbb";
    when(httpRequest.getMethod()).thenReturn(HttpMethod.GET.name());
    when(httpRequest.getPath()).thenReturn(trailingPath);
    ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>> requestHandlers =
        ImmutableMap.of(HttpMethod.GET, ImmutableMap.of(Pattern.compile(goodPath), handler1));
    TestService service = new TestService(requestHandlers);

    service.service(httpRequest, httpResponse);

    verify(httpResponse).setStatusCode(eq(NOT_FOUND.getHttpStatusCode()));
    verify(handler1, never()).handleRequest(any(), any());
    verify(handler2, never()).handleRequest(any(), any());
  }

  public static class TestService extends CloudFunctionServiceBase {
    private final ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
        requestHandlerMap;

    public TestService(
        ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
            requestHandlerMap) {
      this.requestHandlerMap = requestHandlerMap;
    }

    @Override
    protected ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
        getRequestHandlerMap() {
      return requestHandlerMap;
    }
  }
}
