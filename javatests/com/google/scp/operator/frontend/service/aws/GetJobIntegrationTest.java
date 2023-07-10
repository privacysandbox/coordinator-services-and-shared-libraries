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
import static com.google.scp.operator.frontend.service.aws.testing.LocalFrontendServiceModule.GET_JOB_LAMBDA_NAME;
import static com.google.scp.operator.frontend.tasks.ErrorReasons.JOB_NOT_FOUND;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.acai.Acai;
import com.google.inject.Inject;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.operator.frontend.service.aws.testing.FrontendIntegrationTestEnv;
import com.google.scp.operator.protos.frontend.api.v1.GetJobResponseProto.GetJobResponse;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobKeyExistsException;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobMetadataDbException;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.operator.shared.testing.APIGatewayProxyRequestEventFakeFactory;
import com.google.scp.protos.shared.api.v1.ErrorResponseProto.ErrorResponse;
import com.google.scp.shared.mapper.TimeObjectMapper;
import com.google.scp.shared.testutils.aws.LambdaResponsePayload;
import java.util.Map;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.core.SdkBytes;
import software.amazon.awssdk.services.lambda.LambdaClient;
import software.amazon.awssdk.services.lambda.model.InvokeRequest;
import software.amazon.awssdk.services.lambda.model.InvokeResponse;

@RunWith(JUnit4.class)
public final class GetJobIntegrationTest {

  @Rule public Acai acai = new Acai(FrontendIntegrationTestEnv.class);
  private final ObjectMapper mapper = new TimeObjectMapper();
  @Inject private LambdaClient lambdaClient;
  @Inject private DynamoMetadataDb metadataDb;

  @Test(timeout = 120_000)
  public void getJob_success()
      throws JsonProcessingException, JobMetadataDbException, JobKeyExistsException,
          InvalidProtocolBufferException {
    JobMetadata jobMetadata =
        JobGenerator.createFakeJobMetadata("1").toBuilder().clearRecordVersion().build();
    metadataDb.insertJobMetadata(jobMetadata);

    LambdaResponsePayload payload =
        invokeAndValidateLambdaResponse(jobMetadata.getRequestInfo().getJobRequestId());

    assertThat(payload.body()).isNotNull();
    GetJobResponse.Builder builder = GetJobResponse.newBuilder();
    JsonFormat.parser().merge(payload.body(), builder);

    GetJobResponse response = builder.build();
    assertThat(response.getJobRequestId())
        .isEqualTo(jobMetadata.getRequestInfo().getJobRequestId());
    assertThat(payload).isNotNull();
    assertThat(payload.headers().get("content-type")).isEqualTo("application/json");
    assertThat(payload.statusCode()).isEqualTo(200);
  }

  @Test(timeout = 120_000)
  public void getJob_doesNotExist() throws InvalidProtocolBufferException, JsonProcessingException {
    String jobRequestId = "doesnotexist";

    LambdaResponsePayload payload = invokeAndValidateLambdaResponse(jobRequestId);

    ErrorResponse.Builder builder = ErrorResponse.newBuilder();
    JsonFormat.parser().merge(payload.body(), builder);
    ErrorResponse response = builder.build();

    assertThat(response.getCode()).isEqualTo(5);
    assertThat(response.getMessage())
        .isEqualTo("Job with job_request_id '" + jobRequestId + "' could not be found");
    assertThat(response.getDetailsList().get(0).getReason()).isEqualTo(JOB_NOT_FOUND.name());
  }

  private LambdaResponsePayload invokeAndValidateLambdaResponse(String requestId)
      throws JsonProcessingException {
    APIGatewayProxyRequestEvent requestEvent =
        APIGatewayProxyRequestEventFakeFactory.createFromGet(Map.of("job_request_id", requestId));
    InvokeResponse response =
        lambdaClient.invoke(
            InvokeRequest.builder()
                .functionName(GET_JOB_LAMBDA_NAME)
                .payload(SdkBytes.fromUtf8String(mapper.writeValueAsString(requestEvent)))
                .build());

    String responseBody = response.payload().asUtf8String();
    LambdaResponsePayload payload = mapper.readValue(responseBody, LambdaResponsePayload.class);

    return payload;
  }
}
