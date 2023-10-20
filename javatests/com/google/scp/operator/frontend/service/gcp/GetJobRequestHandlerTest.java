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
import static com.google.scp.operator.frontend.tasks.ErrorMessages.JOB_NOT_FOUND_HTML_ESCAPED_MESSAGE;
import static com.google.scp.operator.frontend.tasks.ErrorMessages.JOB_NOT_FOUND_MESSAGE;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.common.collect.ImmutableMap;
import com.google.protobuf.util.JsonFormat;
import com.google.protobuf.util.Timestamps;
import com.google.scp.operator.frontend.service.FrontendService;
import com.google.scp.operator.frontend.tasks.ErrorReasons;
import com.google.scp.operator.protos.frontend.api.v1.GetJobResponseProto.GetJobResponse;
import com.google.scp.operator.protos.frontend.api.v1.JobStatusProto.JobStatus;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.StringWriter;
import java.time.Instant;
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
public final class GetJobRequestHandlerTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private HttpRequest httpRequest;
  @Mock private HttpResponse httpResponse;
  @Mock private FrontendService service;

  private BufferedWriter writerOut;
  private StringWriter httpResponseOut;
  private GetJobRequestHandler requestHandler;

  @Before
  public void setUp() throws IOException {
    requestHandler = new GetJobRequestHandler(service);

    httpResponseOut = new StringWriter();
    writerOut = new BufferedWriter(httpResponseOut);
    when(httpResponse.getWriter()).thenReturn(writerOut);
    when(httpRequest.getMethod()).thenReturn("GET");
  }

  @Test
  public void handleRequest_success() throws Exception {
    String jobRequestId = "1";
    GetJobResponse res =
        GetJobResponse.newBuilder()
            .setJobRequestId(jobRequestId)
            .setJobStatus(JobStatus.IN_PROGRESS)
            .putAllJobParameters(ImmutableMap.of())
            .setInputDataBucketName("inputBucket")
            .setInputDataBlobPrefix("inputPrefix")
            .setOutputDataBucketName("outputBucket")
            .setOutputDataBlobPrefix("outputPrefix")
            .setPostbackUrl("url")
            .setRequestReceivedAt(Timestamps.fromMillis(Instant.now().toEpochMilli()))
            .setRequestUpdatedAt(Timestamps.fromMillis(Instant.now().toEpochMilli()))
            .build();
    when(httpRequest.getFirstQueryParameter(any())).thenReturn(Optional.of(jobRequestId));
    when(service.getJob(jobRequestId)).thenReturn(res);

    requestHandler.handleRequest(httpRequest, httpResponse);
    writerOut.flush();

    JsonFormat.Parser parser = JsonFormat.parser();
    GetJobResponse.Builder protoBuilder = GetJobResponse.newBuilder();
    parser.merge(httpResponseOut.toString(), protoBuilder);

    GetJobResponse response = protoBuilder.build();
    assertThat(response).isEqualTo(res);
    verify(httpResponse).setStatusCode(eq(200));
  }

  @Test
  public void handleRequest_notFoundErrorForInvalidRequestId() throws Exception {
    String jobRequestId = "1";
    when(httpRequest.getFirstQueryParameter(any())).thenReturn(Optional.of(jobRequestId));
    when(service.getJob(jobRequestId))
        .thenThrow(
            new ServiceException(
                Code.NOT_FOUND,
                ErrorReasons.JOB_NOT_FOUND.toString(),
                String.format(JOB_NOT_FOUND_MESSAGE, jobRequestId)));

    requestHandler.handleRequest(httpRequest, httpResponse);
    writerOut.flush();

    assertThat(httpResponseOut.toString())
        .isEqualTo(
            expectedErrorResponseBody(
                /* code= */ NOT_FOUND.getRpcStatusCode(),
                /* message= */ String.format(JOB_NOT_FOUND_HTML_ESCAPED_MESSAGE, jobRequestId),
                /* reason= */ ErrorReasons.JOB_NOT_FOUND.name()));
    verify(httpResponse).setStatusCode(eq(404));
  }

  private static String expectedErrorResponseBody(int code, String message, String reason) {
    return String.format(
        "{\n"
            + "  \"code\": %d,\n"
            + "  \"message\": \"%s\",\n"
            + "  \"details\": [{\n"
            + "    \"reason\": \"%s\",\n"
            + "    \"domain\": \"\",\n"
            + "    \"metadata\": {\n"
            + "    }\n"
            + "  }]\n"
            + "}",
        code, message, reason);
  }
}
