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

package com.google.scp.operator.frontend.service.gcp;

import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.scp.shared.api.model.HttpMethod;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.StringWriter;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class FrontendServiceHttpFunctionTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private HttpRequest httpRequest;
  @Mock private HttpResponse httpResponse;
  @Mock CreateJobRequestHandler createJobRequestHandler;
  @Mock GetJobRequestHandler getJobRequestHandler;
  @Mock PutJobRequestHandler putJobRequestHandler;
  @Mock GetJobByIdRequestHandler getJobByIdRequestHandler;

  private FrontendServiceHttpFunction cloudFunction;
  private int version = 1;

  @Before
  public void setUp() throws IOException {
    cloudFunction =
        new FrontendServiceHttpFunction(
            createJobRequestHandler,
            getJobRequestHandler,
            putJobRequestHandler,
            getJobByIdRequestHandler,
            version);
    StringWriter httpResponseOut = new StringWriter();
    BufferedWriter writerOut = new BufferedWriter(httpResponseOut);
    lenient().when(httpResponse.getWriter()).thenReturn(writerOut);
  }

  @Test
  public void service_createJobApiSupported() throws Exception {
    when(httpRequest.getMethod()).thenReturn(HttpMethod.POST.name());
    when(httpRequest.getPath()).thenReturn("/v1alpha/createJob");

    cloudFunction.service(httpRequest, httpResponse);

    verify(createJobRequestHandler).handleRequest(httpRequest, httpResponse);
    verify(getJobRequestHandler, never()).handleRequest(httpRequest, httpResponse);
  }

  @Test
  public void service_createJobTrailingCharactersFails() throws Exception {
    when(httpRequest.getMethod()).thenReturn(HttpMethod.POST.name());
    when(httpRequest.getPath()).thenReturn("/v1alpha/createJobbbb");

    cloudFunction.service(httpRequest, httpResponse);

    verify(httpResponse).setStatusCode(eq(NOT_FOUND.getHttpStatusCode()));
    verify(getJobRequestHandler, never()).handleRequest(httpRequest, httpResponse);
    verify(createJobRequestHandler, never()).handleRequest(httpRequest, httpResponse);
  }

  @Test
  public void service_getJobApiSupported() throws Exception {
    when(httpRequest.getMethod()).thenReturn(HttpMethod.GET.name());
    when(httpRequest.getPath()).thenReturn("/v1alpha/getJob");

    cloudFunction.service(httpRequest, httpResponse);

    verify(getJobRequestHandler).handleRequest(httpRequest, httpResponse);
    verify(createJobRequestHandler, never()).handleRequest(httpRequest, httpResponse);
  }

  @Test
  public void service_getJobTrailingCharactersFails() throws Exception {
    when(httpRequest.getMethod()).thenReturn(HttpMethod.GET.name());
    when(httpRequest.getPath()).thenReturn("/v1alpha/getJobbbb");

    cloudFunction.service(httpRequest, httpResponse);

    verify(httpResponse).setStatusCode(eq(NOT_FOUND.getHttpStatusCode()));
    verify(getJobRequestHandler, never()).handleRequest(httpRequest, httpResponse);
    verify(createJobRequestHandler, never()).handleRequest(httpRequest, httpResponse);
  }
}
