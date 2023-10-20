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

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_HTTP_METHOD;
import static com.google.scp.shared.api.model.Code.ACCEPTED;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.scp.operator.frontend.service.FrontendService;
import com.google.scp.operator.frontend.tasks.ErrorReasons;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobResponseProto.CreateJobResponse;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.StringReader;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class CreateJobRequestHandlerTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private HttpRequest httpRequest;
  @Mock private HttpResponse httpResponse;
  @Mock private FrontendService service;

  private StringWriter httpResponseOut;
  private BufferedWriter writerOut;
  private CreateJobRequestHandler requestHandler;

  @Before
  public void setUp() throws IOException {
    requestHandler = new CreateJobRequestHandler(service);
    httpResponseOut = new StringWriter();
    writerOut = new BufferedWriter(httpResponseOut);
    when(httpResponse.getWriter()).thenReturn(writerOut);
  }

  @Test
  public void handleRequest_success() throws Exception {
    setRequestBody(getValidFakeRequestJson());

    when(httpRequest.getMethod()).thenReturn("POST");
    when(service.createJob(any())).thenReturn(CreateJobResponse.getDefaultInstance());

    requestHandler.handleRequest(httpRequest, httpResponse);

    writerOut.flush();
    verify(httpResponse).setStatusCode(eq(ACCEPTED.getHttpStatusCode()));
  }

  @Test
  public void handleRequest_invalidJson() throws Exception {
    String jsonRequest = "asdf}{";
    setRequestBody(jsonRequest);
    when(httpRequest.getMethod()).thenReturn("POST");

    requestHandler.handleRequest(httpRequest, httpResponse);

    writerOut.flush();
    String response = httpResponseOut.toString();
    verify(httpResponse).setStatusCode(eq(INVALID_ARGUMENT.getHttpStatusCode()));
    assertThat(response).contains("\"code\": " + INVALID_ARGUMENT.getRpcStatusCode());
    assertThat(response).contains("Expect message object but got: \\\"asdf\\\"");
    assertThat(response).contains("\"reason\": \"" + ErrorReasons.JSON_ERROR.name());
  }

  @Test
  public void handleRequest_invalidHttpMethod() throws Exception {
    setRequestBody(getValidFakeRequestJson());
    when(httpRequest.getMethod()).thenReturn("PUT");

    requestHandler.handleRequest(httpRequest, httpResponse);

    writerOut.flush();
    String response = httpResponseOut.toString();
    verify(httpResponse).setStatusCode(eq(INVALID_ARGUMENT.getHttpStatusCode()));
    assertThat(response).contains("\"code\": " + INVALID_ARGUMENT.getRpcStatusCode());
    assertThat(response).contains("\"message\": \"Unsupported method: \\u0027PUT\\u0027\"");
    assertThat(response).contains("\"reason\": \"" + INVALID_HTTP_METHOD.name());
  }

  @Test
  public void handleRequest_getAccountIdentitySuccess() throws Exception {
    when(httpRequest.getHeaders()).thenReturn(getFakeHttpHeaderWithEmail());
    Optional<String> identity = requestHandler.getAccountIdentityFromHttpHeader(httpRequest);
    assertThat(identity.isPresent()).isTrue();
    assertThat(identity.get()).isEqualTo("account@project.iam.gserviceaccount.com");
  }

  @Test
  public void handleRequest_getAccountIdentityMissingEmail() throws Exception {
    when(httpRequest.getHeaders()).thenReturn(getFakeHttpHeaderMissingEmail());
    Optional<String> identity = requestHandler.getAccountIdentityFromHttpHeader(httpRequest);
    assertThat(identity.isPresent()).isFalse();
  }

  @Test
  public void handleRequest_getAccountIdentityMissingAuth() throws Exception {
    when(httpRequest.getHeaders()).thenReturn(new HashMap<String, List<String>>());
    Optional<String> identity = requestHandler.getAccountIdentityFromHttpHeader(httpRequest);
    assertThat(identity.isPresent()).isFalse();
  }

  private String getValidFakeRequestJson() {
    String id = "id";
    String inPrefix = "inPrefix";
    String inBucket = "inBucket";
    String outPrefix = "outPrefix";
    String outBucket = "outBucket";
    String postback = "postbackUrl";
    String jsonRequest =
        "{\"job_request_id\":\""
            + id
            + "\",\"input_data_blob_prefix\":\""
            + inPrefix
            + "\",\"input_data_bucket_name\":\""
            + inBucket
            + "\",\"output_data_blob_prefix\":\""
            + outPrefix
            + "\",\"output_data_bucket_name\":\""
            + outBucket
            + "\",\"postback_url\":\""
            + postback
            + "\",\"job_parameters\":{\"attribution_report_to\":\"foo.com\"}}";

    return jsonRequest;
  }

  private Map<String, List<String>> getFakeHttpHeaderWithEmail() {
    Map<String, List<String>> httpHeader = new HashMap<String, List<String>>();
    httpHeader.put(
        "Authorization",
        Arrays.asList(
            "bearer"
                + " eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJodHRwczovL2FjY291bnRzLmdvb2dsZS5jb20iLCJhenAiOiIxMTIyMzM0NDU1Ni5hcHBzLmdvb2dsZXVzZXJjb250ZW50LmNvbSIsImF1ZCI6IjExMjIzMzQ0NTU2LmFwcHMuZ29vZ2xldXNlcmNvbnRlbnQuY29tIiwic3ViIjoiMDEyMzQ1Njc4OTAxMjM0NTY3ODkwIiwiaGQiOiJnb29nbGUuY29tIiwiZW1haWwiOiJhY2NvdW50QHByb2plY3QuaWFtLmdzZXJ2aWNlYWNjb3VudC5jb20iLCJlbWFpbF92ZXJpZmllZCI6dHJ1ZSwiYXRfaGFzaCI6ImhxakVGYThIdDRTRUp1NFdiX3dENDAiLCJpYXQiOjE2ODMyNDIxODUsImV4cCI6MTY4MzI0NTc4NX0.SIGNATURE_REMOVED_BY_GOOGLE"));
    return httpHeader;
  }

  private Map<String, List<String>> getFakeHttpHeaderMissingEmail() {
    Map<String, List<String>> httpHeader = new HashMap<String, List<String>>();
    httpHeader.put(
        "Authorization",
        Arrays.asList(
            "bearer"
                + " eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJodHRwczovL2FjY291bnRzLmdvb2dsZS5jb20iLCJhenAiOiIxMTIyMzM0NDU1Ni5hcHBzLmdvb2dsZXVzZXJjb250ZW50LmNvbSIsImF1ZCI6IjExMjIzMzQ0NTU2LmFwcHMuZ29vZ2xldXNlcmNvbnRlbnQuY29tIiwic3ViIjoiMDEyMzQ1Njc4OTAxMjM0NTY3ODkwIiwiaGQiOiJnb29nbGUuY29tIiwiZW1haWxfdmVyaWZpZWQiOnRydWUsImF0X2hhc2giOiJocWpFRmE4SHQ0U0VKdTRXYl93RDQwIiwiaWF0IjoxNjgzMjQyMTg1LCJleHAiOjE2ODMyNDU3ODV9.SIGNATURE_REMOVED_BY_GOOGLE"));
    return httpHeader;
  }

  private void setRequestBody(String body) throws IOException {
    BufferedReader reader = new BufferedReader(new StringReader(body));
    lenient().when(httpRequest.getReader()).thenReturn(reader);
  }
}
