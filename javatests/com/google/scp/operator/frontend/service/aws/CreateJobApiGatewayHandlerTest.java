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

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.common.collect.ImmutableMap;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.scp.operator.frontend.injection.factories.FrontendServicesFactory;
import com.google.scp.operator.frontend.injection.modules.testing.FakeFrontendModule;
import com.google.scp.operator.frontend.service.ServiceJobGenerator;
import com.google.scp.operator.frontend.tasks.ErrorReasons;
import com.google.scp.operator.frontend.testing.FakeRequestInfoValidator;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestProto.CreateJobRequest;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.testing.FakeMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.operator.shared.testing.APIGatewayProxyRequestEventFakeFactory;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.Details;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.api.model.Code;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;
import java.util.stream.IntStream;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class CreateJobApiGatewayHandlerTest {

  private static final String RESOURCES_DIR =
      "javatests/com/google/scp/operator/frontend/service/aws/resources/";
  private static final String INVALID_ARGUMENT_JSON = "invalid_argument.json";
  private static final String MISSING_REQUIRED_PROPERTY = "missing_required_property.json";
  private CreateJobApiGatewayHandler target;
  private FakeMetadataDb jobMetadataDb;
  private FakeRequestInfoValidator fakeRequestInfoValidator;

  @Before
  public void setUp() {
    target = new CreateJobApiGatewayHandler();
    jobMetadataDb =
        (FakeMetadataDb) FrontendServicesFactory.getInjector().getInstance(JobMetadataDb.class);
    fakeRequestInfoValidator = FakeFrontendModule.getFakeRequestInfoValidator();
    fakeRequestInfoValidator.setValidateReturnValue(Optional.empty());
    jobMetadataDb.reset();
  }

  @Test
  public void createJob_returnsAccepted() {
    CreateJobRequest createJobRequest = ServiceJobGenerator.createFakeCreateJobRequest("123");

    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest), null);
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThat(responseEvent.getBody()).isEqualTo("{\n}");
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.ACCEPTED.getHttpStatusCode());
    assertThat(jobMetadataDb.getLastJobMetadataInserted().getRequestInfo())
        .isEqualTo(JobGenerator.createFakeRequestInfo("123"));
  }

  @Test
  public void createJob_returnsInvalidArgument_whenArgumentInvalid()
      throws InvalidProtocolBufferException {
    CreateJobRequest createJobRequest = ServiceJobGenerator.createFakeCreateJobRequest("123");
    String validationErrorMessage = "Oh no a validation failure";
    fakeRequestInfoValidator.setValidateReturnValue(Optional.of(validationErrorMessage));

    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest), null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage(validationErrorMessage)
            .addAllDetails(
                List.of(
                    Details.newBuilder()
                        .setReason(ErrorReasons.VALIDATION_FAILED.toString())
                        .build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThatResponseBodyContains(responseEvent, errorResponse);
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
    assertThat(jobMetadataDb.getLastJobMetadataInserted()).isNull();
  }

  @Test
  public void createJob_returnsUnknownError_whenExceptionThrown()
      throws InvalidProtocolBufferException {
    jobMetadataDb.setShouldThrowJobMetadataDbException(true);
    CreateJobRequest createJobRequest = ServiceJobGenerator.createFakeCreateJobRequest("123");

    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest), null);

    ErrorResponse expectedErrorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INTERNAL.getRpcStatusCode())
            .setMessage("Internal error occurred when reaching the DB")
            .addAllDetails(
                List.of(
                    Details.newBuilder().setReason(ErrorReasons.SERVER_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThatResponseBodyContains(responseEvent, expectedErrorResponse);
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.UNKNOWN.getHttpStatusCode());
    assertThat(jobMetadataDb.getLastJobMetadataInserted()).isNull();
  }

  @Test
  public void createJob_returnsInvalidArgument_whenJsonInvalid()
      throws InvalidProtocolBufferException {
    APIGatewayProxyRequestEvent request = new APIGatewayProxyRequestEvent();
    request.setBody("{invalidJson");
    APIGatewayProxyResponseEvent responseEvent = target.handleRequest(request, null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage("java.io.EOFException: End of input at line 1 column 13 path $.invalidJson")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThatResponseBodyContains(responseEvent, errorResponse);
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void createJob_returns_conflictError_whenJobAlreadyExists()
      throws InvalidProtocolBufferException {
    CreateJobRequest createJobRequest = ServiceJobGenerator.createFakeCreateJobRequest("123");
    jobMetadataDb.setShouldThrowJobKeyExistsException(true);
    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest), null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.ALREADY_EXISTS.getRpcStatusCode())
            .setMessage(
                String.format(
                    "Duplicate job_request_id provided: job_request_id=%s is not unique.",
                    createJobRequest.getJobRequestId()))
            .addAllDetails(
                List.of(
                    Details.newBuilder()
                        .setReason(ErrorReasons.DUPLICATE_JOB_KEY.toString())
                        .build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThatResponseBodyContains(responseEvent, errorResponse);
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.ALREADY_EXISTS.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSpecificError_whenUnrecognizedField() throws IOException {
    APIGatewayProxyRequestEvent request = new APIGatewayProxyRequestEvent();
    request.setBody(Files.readString(Path.of(RESOURCES_DIR, INVALID_ARGUMENT_JSON)));
    request.setHttpMethod("POST");

    APIGatewayProxyResponseEvent responseEvent = target.handleRequest(request, null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage(
                "Cannot find field: job_request_id_ in"
                    + " message google.scp.operator.protos.frontend.api.v1.CreateJobRequest")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThatResponseBodyContains(responseEvent, errorResponse);
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSpecificError_whenRequiredPropertiesMissing()
      throws InvalidProtocolBufferException {
    APIGatewayProxyRequestEvent request = new APIGatewayProxyRequestEvent();
    request.setBody("{}");
    request.setHttpMethod("POST");

    APIGatewayProxyResponseEvent responseEvent = target.handleRequest(request, null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage(
                "Missing required properties: jobRequestId"
                    + " inputDataBucketName outputDataBlobPrefix outputDataBucketName\r\n in: {}")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThatResponseBodyContains(responseEvent, errorResponse);
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSpecificError_whenRequiredPropertyMissing() throws IOException {
    APIGatewayProxyRequestEvent request = new APIGatewayProxyRequestEvent();
    request.setBody(Files.readString(Path.of(RESOURCES_DIR, MISSING_REQUIRED_PROPERTY)));
    request.setHttpMethod("POST");

    APIGatewayProxyResponseEvent responseEvent = target.handleRequest(request, null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage(
                "Missing required properties: jobRequestId outputDataBlobPrefix"
                    + " outputDataBucketName\r\n"
                    + " in: {\n"
                    + "  \"input_data_bucket_name\": \"bucket\",\n"
                    + "  \"postback_url\": \"http://postback.com\",\n"
                    + "  \"input_data_blob_prefix\": \"dataHandle\"\n"
                    + "}")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThatResponseBodyContains(responseEvent, errorResponse);
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSpecificError_whenBothInputPrefixAndInputPrefixListMissing() {
    CreateJobRequest createJobRequest = ServiceJobGenerator.createFakeCreateJobRequest("123");
    createJobRequest = createJobRequest.toBuilder().setInputDataBlobPrefix("").build();
    APIGatewayProxyRequestEvent request =
        APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest);

    APIGatewayProxyResponseEvent responseEvent = target.handleRequest(request, null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage(
                "Exactly one of the properties input_data_blob_prefix and"
                    + " input_data_blob_prefixes must be provided")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThat(responseEvent.getBody()).contains(errorResponse.getMessage());
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSpecificError_whenBothInputPrefixAndInputPrefixListProvided() {
    CreateJobRequest createJobRequest = ServiceJobGenerator.createFakeCreateJobRequest("123");
    createJobRequest =
        createJobRequest.toBuilder()
            .addAllInputDataBlobPrefixes(List.of("prefix1", "prefix2"))
            .build();
    APIGatewayProxyRequestEvent request =
        APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest);

    APIGatewayProxyResponseEvent responseEvent = target.handleRequest(request, null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage(
                "Exactly one of the properties input_data_blob_prefix and"
                    + " input_data_blob_prefixes must be provided")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThat(responseEvent.getBody()).contains(errorResponse.getMessage());
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSpecificError_whenInputPrefixListLongerThanMaxLimit() {
    CreateJobRequest createJobRequest = ServiceJobGenerator.createFakeCreateJobRequest("123");
    List<String> tooManyInputPrefixes =
        IntStream.range(0, 51).boxed().map(Object::toString).collect(Collectors.toList());
    createJobRequest =
        createJobRequest.toBuilder()
            .setInputDataBlobPrefix("")
            .addAllInputDataBlobPrefixes(tooManyInputPrefixes)
            .build();
    APIGatewayProxyRequestEvent request =
        APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest);

    APIGatewayProxyResponseEvent responseEvent = target.handleRequest(request, null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage("Property input_data_blob_prefixes should contain a maximum of 50 items:")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThat(responseEvent.getBody()).contains(errorResponse.getMessage());
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSpecificError_whenReportingSiteAndAttributionReportToBothPresent() {
    ImmutableMap<String, String> jobParams =
        ImmutableMap.of("attribution_report_to", "someOrigin", "reporting_site", "someSite");
    CreateJobRequest createJobRequest =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters("123", jobParams);

    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest), null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage(
                "Exactly one of attribution_report_to and reporting_site fields should be specified"
                    + " for the job.")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThat(responseEvent.getBody()).contains(errorResponse.getMessage());
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void
      createJob_returnsSpecificError_whenBothReportingSiteAndAttributionReportToNotPresent() {
    ImmutableMap<String, String> jobParams = ImmutableMap.of();
    CreateJobRequest createJobRequest =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters("123", jobParams);

    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest), null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage(
                "Exactly one of attribution_report_to and reporting_site fields should be specified"
                    + " for the job.")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThat(responseEvent.getBody()).contains(errorResponse.getMessage());
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSpecificError_whenAttributionReportToPresentButEmpty() {
    ImmutableMap<String, String> jobParams = ImmutableMap.of("attribution_report_to", "");
    CreateJobRequest createJobRequest =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters("123", jobParams);

    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest), null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage("The attribution_report_to field in the job parameters is empty:")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThat(responseEvent.getBody()).contains(errorResponse.getMessage());
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSpecificError_whenAttributionReportToPresentButMultipleValues() {
    ImmutableMap<String, String> jobParams =
        ImmutableMap.of("attribution_report_to", "foo.com, bar.com");
    CreateJobRequest createJobRequest =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters("123", jobParams);

    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest), null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage(
                "The attribution_report_to field in the job parameters should contain a single"
                    + " value:")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThat(responseEvent.getBody()).contains(errorResponse.getMessage());
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSpecificError_whenReportingSitePresentButEmpty() {
    ImmutableMap<String, String> jobParams = ImmutableMap.of("reporting_site", "");
    CreateJobRequest createJobRequest =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters("123", jobParams);

    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest), null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage("The reporting_site field in the job parameters is empty:")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThat(responseEvent.getBody()).contains(errorResponse.getMessage());
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSpecificError_whenReportingSitePresentButMultipleValues() {
    ImmutableMap<String, String> jobParams = ImmutableMap.of("reporting_site", "foo.com, bar.com");
    CreateJobRequest createJobRequest =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters("123", jobParams);

    APIGatewayProxyResponseEvent responseEvent =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(createJobRequest), null);

    ErrorResponse errorResponse =
        ErrorResponse.newBuilder()
            .setCode(Code.INVALID_ARGUMENT.getRpcStatusCode())
            .setMessage(
                "The reporting_site field in the job parameters should contain a single value:")
            .addAllDetails(
                List.of(Details.newBuilder().setReason(ErrorReasons.JSON_ERROR.toString()).build()))
            .build();
    assertThat(responseEvent.getHeaders().get("content-type")).isEqualTo("application/json");
    assertThat(responseEvent.getBody()).contains(errorResponse.getMessage());
    assertThat(responseEvent.getStatusCode()).isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSuccess_validInputReportCount() {
    ImmutableMap<String, String> jobParams = ImmutableMap.of("reporting_site", "foo.com");
    CreateJobRequest createJobRequestWithoutCount =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters("123", jobParams);
    ImmutableMap<String, String> jobParamsWithEmptyString =
        ImmutableMap.of("reporting_site", "foo.com", "input_report_count", " ");
    CreateJobRequest createJobRequestWithEmptyString =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters(
            "123", jobParamsWithEmptyString);
    ImmutableMap<String, String> jobParamsWithTrailingSpace =
        ImmutableMap.of("reporting_site", "foo.com", "input_report_count", "100    ");
    CreateJobRequest createJobRequestWithTrailingSpace =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters(
            "123", jobParamsWithTrailingSpace);
    ImmutableMap<String, String> jobParamsWithZeroReportCount =
        ImmutableMap.of("reporting_site", "foo.com", "input_report_count", "0");
    CreateJobRequest createJobRequestWithZeroReportCount =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters(
            "123", jobParamsWithZeroReportCount);

    APIGatewayProxyResponseEvent responseEventWithoutCount =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(
                createJobRequestWithoutCount),
            null);
    APIGatewayProxyResponseEvent responseEventWithEmptyString =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(
                createJobRequestWithEmptyString),
            null);
    APIGatewayProxyResponseEvent responseEventWithTrailingSpace =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(
                createJobRequestWithTrailingSpace),
            null);
    APIGatewayProxyResponseEvent responseEventWithZeroReportCount =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(
                createJobRequestWithZeroReportCount),
            null);

    assertThat(responseEventWithoutCount.getStatusCode())
        .isEqualTo(Code.ACCEPTED.getHttpStatusCode());
    assertThat(responseEventWithEmptyString.getStatusCode())
        .isEqualTo(Code.ACCEPTED.getHttpStatusCode());
    assertThat(responseEventWithTrailingSpace.getStatusCode())
        .isEqualTo(Code.ACCEPTED.getHttpStatusCode());
    assertThat(responseEventWithZeroReportCount.getStatusCode())
        .isEqualTo(Code.ACCEPTED.getHttpStatusCode());
  }

  @Test
  public void createJob_returnsSpecificError_invalidInputReportCount() {
    ImmutableMap<String, String> jobParamsNegativeReportCount =
        ImmutableMap.of("reporting_site", "foo.com", "input_report_count", "-1");
    CreateJobRequest createJobRequestNegativeReportCount =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters(
            "123", jobParamsNegativeReportCount);
    ImmutableMap<String, String> jobParamsNaNReportCount =
        ImmutableMap.of("reporting_site", "foo.com", "input_report_count", "not a number");
    CreateJobRequest createJobRequestNaNReportCount =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters(
            "123", jobParamsNaNReportCount);
    ImmutableMap<String, String> jobParamsFractionalReportCount =
        ImmutableMap.of("reporting_site", "foo.com", "input_report_count", "100.1");
    CreateJobRequest createJobRequestFractionalReportCount =
        ServiceJobGenerator.createFakeCreateJobRequestWithJobParameters(
            "123", jobParamsFractionalReportCount);

    APIGatewayProxyResponseEvent responseEventNegativeReportCount =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(
                createJobRequestNegativeReportCount),
            null);
    APIGatewayProxyResponseEvent responseEventNaNReportCount =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(
                createJobRequestNaNReportCount),
            null);
    APIGatewayProxyResponseEvent responseEventFractionalReportCount =
        target.handleRequest(
            APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(
                createJobRequestFractionalReportCount),
            null);

    assertThat(responseEventNegativeReportCount.getBody())
        .contains("Job parameter input_report_count should have a valid non-negative value:");
    assertThat(responseEventNegativeReportCount.getStatusCode())
        .isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
    assertThat(responseEventNaNReportCount.getBody())
        .contains("Job parameter input_report_count should have a valid non-negative value:");
    assertThat(responseEventNaNReportCount.getStatusCode())
        .isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
    assertThat(responseEventFractionalReportCount.getBody())
        .contains("Job parameter input_report_count should have a valid non-negative value:");
    assertThat(responseEventFractionalReportCount.getStatusCode())
        .isEqualTo(Code.INVALID_ARGUMENT.getHttpStatusCode());
  }
}
