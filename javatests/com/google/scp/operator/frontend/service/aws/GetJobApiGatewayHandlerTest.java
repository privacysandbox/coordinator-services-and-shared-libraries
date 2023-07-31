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

package com.google.scp.operator.frontend.service.aws;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.operator.frontend.testing.ResponseEventAssertions.assertThatResponseBodyContains;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.common.collect.ImmutableMap;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.operator.frontend.injection.factories.FrontendServicesFactory;
import com.google.scp.operator.frontend.injection.modules.testing.FakeFrontendModule;
import com.google.scp.operator.frontend.service.ServiceJobGenerator;
import com.google.scp.operator.frontend.tasks.ErrorReasons;
import com.google.scp.operator.frontend.testing.FakeRequestInfoValidator;
import com.google.scp.operator.protos.frontend.api.v1.GetJobResponseProto.GetJobResponse;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.testing.FakeMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.operator.shared.testing.APIGatewayProxyRequestEventFakeFactory;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.Details;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.model.Code;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class GetJobApiGatewayHandlerTest {

  private GetJobApiGatewayHandler target;
  private FakeMetadataDb jobMetadataDb;
  private FakeRequestInfoValidator fakeRequestInfoValidator;

  @Before
  public void setUp() {
    target = new GetJobApiGatewayHandler();
    jobMetadataDb =
        (FakeMetadataDb) FrontendServicesFactory.getInjector().getInstance(JobMetadataDb.class);
    fakeRequestInfoValidator = FakeFrontendModule.getFakeRequestInfoValidator();
    fakeRequestInfoValidator.setValidateReturnValue(Optional.empty());
    jobMetadataDb.reset();
  }

  @Test
  public void jobReturnedInResponse_whenGetJobRequestReceived()
      throws InvalidProtocolBufferException {
    JobMetadata jobMetadata = JobGenerator.createFakeJobMetadata("123");
    jobMetadataDb.setJobMetadataToReturn(Optional.of(jobMetadata));

    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromGet(
                Map.of("job_request_id", "123", "attribution_report_to", "foo.com")),
            null);

    GetJobResponse response = ServiceJobGenerator.createFakeJobResponse("123");

    GetJobResponse.Builder builder = GetJobResponse.newBuilder();
    JsonFormat.parser().merge(responseEvent.getBody(), builder);
    assertThatResponseBodyContains(builder.build(), response);
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.OK.getHttpStatusCode());
    assertThat(jobMetadataDb.getLastJobKeyStringLookedUp())
        .isEqualTo(jobMetadata.getJobKey().getJobRequestId());
  }

  @Test
  public void unknownErrorResponseReturned_whenExceptionThrownDuringGetJobRequest()
      throws InvalidProtocolBufferException {
    jobMetadataDb.setShouldThrowJobMetadataDbException(true);

    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromGet(
                Map.of("job_request_id", "123", "attribution_report_to", "foo.com")),
            null);

    ErrorResponse expectedErrorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INTERNAL.getRpcStatusCode())
            .setMessage("Internal error occurred when reaching the DB")
            .addAllDetails(
                List.of(
                    Details.newBuilder().setReason(ErrorReasons.SERVER_ERROR.toString()).build()))
            .build();

    assertThatResponseBodyContains(responseEvent, expectedErrorResponse);
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INTERNAL.getHttpStatusCode());
  }

  @Test
  public void notFoundResponseReturned_whenJobNotFound() throws InvalidProtocolBufferException {
    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromGet(
                Map.of("job_request_id", "123", "attribution_report_to", "foo.com")),
            null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.NOT_FOUND.getRpcStatusCode())
            .setMessage("Job with job_request_id '123' could" + " not be found")
            .addAllDetails(
                List.of(
                    Details.newBuilder().setReason(ErrorReasons.JOB_NOT_FOUND.toString()).build()))
            .build();

    assertThatResponseBodyContains(responseEvent, errorResponse);
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.NOT_FOUND.getHttpStatusCode());
    assertThat(jobMetadataDb.getLastJobKeyStringLookedUp()).isEqualTo("123");
  }

  @Test
  public void jobRequestIdNotSpecified_ReturnsInvalidArgument()
      throws InvalidProtocolBufferException {
    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromGet(
                Map.of("attribution_report_to", "foo.com")),
            null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage("Missing required query parameters: [job_request_id] must be specified")
            .addAllDetails(
                List.of(
                    Details.newBuilder()
                        .setReason(ErrorReasons.ARGUMENT_MISSING.toString())
                        .build()))
            .build();

    assertThatResponseBodyContains(responseEvent, errorResponse);
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void neitherParamSpecified_ReturnsInvalidArgument() throws InvalidProtocolBufferException {
    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromGet(ImmutableMap.of()), null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage(
                "Missing required query parameters: [job_request_id]" + " must be specified")
            .addAllDetails(
                List.of(
                    Details.newBuilder()
                        .setReason(ErrorReasons.ARGUMENT_MISSING.toString())
                        .build()))
            .build();

    assertThatResponseBodyContains(responseEvent, errorResponse);
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }
}
