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

import static com.google.scp.shared.api.model.Code.FAILED_PRECONDITION;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static com.google.scp.shared.api.model.Code.OK;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.common.collect.ImmutableMap;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.HttpMethod;
import io.opentelemetry.api.OpenTelemetry;
import io.opentelemetry.api.common.AttributeKey;
import io.opentelemetry.sdk.common.InstrumentationScopeInfo;
import io.opentelemetry.sdk.testing.assertj.OpenTelemetryAssertions;
import io.opentelemetry.sdk.testing.junit4.OpenTelemetryRule;
import io.opentelemetry.semconv.HttpAttributes;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;
import org.junit.Assert;
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
  @Rule public final OpenTelemetryRule testOTel = OpenTelemetryRule.create();

  @Mock private CloudFunctionRequestHandler handler1;
  @Mock private CloudFunctionRequestHandler handler2;
  @Mock private HttpRequest httpRequest;
  @Mock private HttpResponse httpResponse;
  private static final String PATH1 = "/v1/createJob";
  private static final String PATH2 = "/v1/getJob";
  private static final String PATH3 = "/v1/deleteJob";

  /**
   * Created a mock JWT token for testing { "iss": "Online JWT Builder", "iat": 1729628364, "exp":
   * 1761164364, "aud": "www.testcoordinator.com", "sub": "12345", "email": "testemail.com" }
   */
  private static final Map<String, List<String>> authHeaders =
      generateTestAuthHeader(
          "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJPbmxpbmUgSldUIEJ1aWxkZXIiLCJpYXQiOjE3Mjk2MjgzNjQsImV4cCI6MTc2MTE2NDM2NCwiYXVkIjoid3d3LnRlc3Rjb29yZGluYXRvci5jb20iLCJzdWIiOiIxMjM0NSIsImVtYWlsIjoidGVzdGVtYWlsLmNvbSJ9.dQCATA-LyQ-Z1DVz42tX7ry15R8XMPLDrnkXRESNoBI");

  // Same as above, except email field is missing.
  private static final Map<String, List<String>> authHeadersNoEmail =
      generateTestAuthHeader(
          "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJPbmxpbmUgSldUIEJ1aWxkZXIiLCJpYXQiOjE3Mjk2MjgzNjQsImV4cCI6MTc2MTE2NDM2NCwiYXVkIjoid3d3LnRlc3Rjb29yZGluYXRvci5jb20iLCJzdWIiOiIxMjM0NSJ9.SN1b3z3KT_wVoHwqyIZ-yCGNjFrTTDU4Db7ee8yplpA");

  // Invalid Header without space in the middle
  private static final Map<String, List<String>> authHeaderBadFormat =
      generateTestAuthHeader(
          "BearereyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJPbmxpbmUgSldUIEJ1aWxkZXIiLCJpYXQiOjE3Mjk2MjgzNjQsImV4cCI6MTc2MTE2NDM2NCwiYXVkIjoid3d3LnRlc3Rjb29yZGluYXRvci5jb20iLCJzdWIiOiIxMjM0NSJ9.SN1b3z3KT_wVoHwqyIZ-yCGNjFrTTDU4Db7ee8yplpA");

  // Valid header format but bad token.
  private static final Map<String, List<String>> authHeaderBadToken =
      generateTestAuthHeader("Bearer imabadtokenskibidirizzohiosigma");

  /**
   * @param testToken A JWT token for testing purposes
   * @return An object in the return type of {@link com.google.cloud.functions.HttpMessage}'s
   *     getHeaders() function.
   */
  private static Map<String, List<String>> generateTestAuthHeader(String testToken) {
    return Map.of("Authorization", List.of("Bearer " + testToken));
  }

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

  @Test
  public void service_testOTelMetrics() throws IOException {
    when(httpRequest.getMethod()).thenReturn(HttpMethod.GET.name());
    when(httpRequest.getPath()).thenReturn(PATH2);
    HttpResponseWrapper httpResponseWrapper = HttpResponseWrapper.from(httpResponse);
    // Mock the handler so that it sets the statusCode of the wrapper.
    doAnswer(
            invocationOnMock -> {
              ((HttpResponseWrapper) invocationOnMock.getArgument(1))
                  .setStatusCode(OK.getHttpStatusCode());
              return null;
            })
        .when(handler1)
        .handleRequest(httpRequest, httpResponseWrapper);

    ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>> requestHandlers =
        ImmutableMap.of(HttpMethod.GET, ImmutableMap.of(Pattern.compile(PATH2), handler1));
    TestService service = new TestService(requestHandlers, testOTel.getOpenTelemetry());

    List<Map<String, List<String>>> headerTestCases =
        Arrays.asList(
            null, // Should record w/o client ID.
            authHeaders, // Should record w/ email and subject.
            authHeadersNoEmail, // Should record w/ ONLY subject.
            authHeaderBadFormat, // Should record w/o client ID.
            authHeaderBadToken // Should record w/o client ID.
            );

    headerTestCases.forEach(
        testCase -> {
          try {
            if (testCase != null) {
              when(httpRequest.getHeaders()).thenReturn(testCase);
            }
            service.service(httpRequest, httpResponseWrapper);
          } catch (Exception e) {
            throw new RuntimeException(e);
          }
        });

    OpenTelemetryAssertions.assertThat(testOTel.getMetrics())
        .satisfiesExactly(
            metricData -> {
              InstrumentationScopeInfo scopeInfo =
                  InstrumentationScopeInfo.builder(
                          "com.google.scp.shared.gcp.util.CloudFunctionServiceBaseTest$TestService")
                      .build();
              OpenTelemetryAssertions.assertThat(metricData)
                  .hasName("http.server.request.duration")
                  .hasHistogramSatisfying(
                      hist ->
                          hist.isCumulative()
                              .hasPointsSatisfying(
                                  pointsWithoutID ->
                                      pointsWithoutID
                                          .hasCount(3)
                                          .hasAttribute(
                                              HttpAttributes.HTTP_RESPONSE_STATUS_CODE, 200L)
                                          .hasAttribute(HttpAttributes.HTTP_REQUEST_METHOD, "GET"),
                                  pointWithEmail ->
                                      pointWithEmail
                                          .hasAttribute(
                                              AttributeKey.stringKey("client_email"),
                                              "testemail.com")
                                          .hasAttribute(
                                              AttributeKey.stringKey("client_subject"), "12345")
                                          .hasAttribute(
                                              HttpAttributes.HTTP_RESPONSE_STATUS_CODE, 200L),
                                  pointWithoutEmail ->
                                      pointWithoutEmail
                                          .hasAttribute(
                                              AttributeKey.stringKey("client_subject"), "12345")
                                          .hasAttribute(
                                              HttpAttributes.HTTP_RESPONSE_STATUS_CODE, 200L)))
                  .hasInstrumentationScope(scopeInfo);
            });
  }

  @Test
  public void service_invalidPathFailsMetric() throws Exception {
    when(httpRequest.getMethod()).thenReturn(HttpMethod.GET.name());
    when(httpRequest.getPath()).thenReturn(PATH3);

    ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>> requestHandlers =
        ImmutableMap.of(
            HttpMethod.GET,
            ImmutableMap.of(Pattern.compile(PATH2), handler1),
            HttpMethod.POST,
            ImmutableMap.of(Pattern.compile(PATH1), handler2));
    TestService service = new TestService(requestHandlers, testOTel.getOpenTelemetry());
    HttpResponseWrapper httpResponseWrapper = HttpResponseWrapper.from(httpResponse);

    // First execute tests that invalid path (PATH3) will have 404 status code in metric.
    service.service(httpRequest, httpResponseWrapper);
    when(httpRequest.getMethod()).thenReturn("DELETE");
    // Second execute tests that invalid http Method will have 400 status code in metric.
    service.service(httpRequest, httpResponseWrapper);
    OpenTelemetryAssertions.assertThat(testOTel.getMetrics())
        .satisfiesExactly(
            metricData -> {
              InstrumentationScopeInfo scopeInfo =
                  InstrumentationScopeInfo.builder(
                          "com.google.scp.shared.gcp.util.CloudFunctionServiceBaseTest$TestService")
                      .build();
              OpenTelemetryAssertions.assertThat(metricData)
                  .hasName("http.server.request.duration")
                  .hasHistogramSatisfying(
                      hist ->
                          hist.isCumulative()
                              .hasPointsSatisfying(
                                  notFoundPathPoint ->
                                      notFoundPathPoint
                                          .hasCount(1)
                                          .hasAttribute(
                                              HttpAttributes.HTTP_RESPONSE_STATUS_CODE, 404L),
                                  notFoundMethodPoint ->
                                      notFoundMethodPoint
                                          .hasCount(1)
                                          .hasAttribute(
                                              HttpAttributes.HTTP_RESPONSE_STATUS_CODE, 400L)))
                  .hasInstrumentationScope(scopeInfo);
            });
  }

  @Test
  public void service_handleRequestExceptionMetric() throws Exception {
    when(httpRequest.getMethod()).thenReturn(HttpMethod.GET.name());
    when(httpRequest.getPath()).thenReturn(PATH2);

    ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>> requestHandlers =
        ImmutableMap.of(HttpMethod.GET, ImmutableMap.of(Pattern.compile(PATH2), handler1));
    TestService service = new TestService(requestHandlers, testOTel.getOpenTelemetry());
    HttpResponseWrapper httpResponseWrapper = HttpResponseWrapper.from(httpResponse);
    // For unchecked exception, expect OTel metric still being recorded and error code is set to
    // 500.
    doAnswer(
            invocationOnMock -> {
              ((HttpResponseWrapper) invocationOnMock.getArgument(1))
                  .setStatusCode(INTERNAL.getHttpStatusCode());
              throw new RuntimeException();
            })
        .when(handler1)
        .handleRequest(any(HttpRequest.class), any(HttpResponse.class));
    Assert.assertThrows(
        RuntimeException.class, () -> service.service(httpRequest, httpResponseWrapper));

    // For checked exceptions, expect OTel metric still being recorded and error code is set to the
    // error code set by the checked exception handler.
    doAnswer(
            invocationOnMock -> {
              ((HttpResponseWrapper) invocationOnMock.getArgument(1))
                  .setStatusCode(FAILED_PRECONDITION.getHttpStatusCode());
              throw new ServiceException(FAILED_PRECONDITION, "testError", "testMsg");
            })
        .when(handler1)
        .handleRequest(any(HttpRequest.class), any(HttpResponse.class));
    Assert.assertThrows(
        ServiceException.class, () -> service.service(httpRequest, httpResponseWrapper));
    OpenTelemetryAssertions.assertThat(testOTel.getMetrics())
        .satisfiesExactly(
            metricData -> {
              InstrumentationScopeInfo scopeInfo =
                  InstrumentationScopeInfo.builder(
                          "com.google.scp.shared.gcp.util.CloudFunctionServiceBaseTest$TestService")
                      .build();
              OpenTelemetryAssertions.assertThat(metricData)
                  .hasName("http.server.request.duration")
                  .hasHistogramSatisfying(
                      hist ->
                          hist.isCumulative()
                              .hasPointsSatisfying(
                                  uncheckedExceptionMetric ->
                                      uncheckedExceptionMetric
                                          .hasCount(1)
                                          .hasAttribute(
                                              HttpAttributes.HTTP_RESPONSE_STATUS_CODE, 500L),
                                  checkedExceptionMetric ->
                                      checkedExceptionMetric
                                          .hasCount(1)
                                          .hasAttribute(
                                              HttpAttributes.HTTP_RESPONSE_STATUS_CODE, 400L)))
                  .hasInstrumentationScope(scopeInfo);
            });
  }

  public static class TestService extends CloudFunctionServiceBase {
    private final ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
        requestHandlerMap;

    public TestService(
        ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
            requestHandlerMap,
        OpenTelemetry OTel) {
      super(OTel);
      this.requestHandlerMap = requestHandlerMap;
    }

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
