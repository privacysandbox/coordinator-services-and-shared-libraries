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
import static com.google.scp.operator.frontend.service.aws.testing.LocalFrontendServiceModule.CREATE_JOB_LAMBDA_NAME;
import static com.google.scp.operator.frontend.service.model.Constants.JOB_PARAM_ATTRIBUTION_REPORT_TO;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.acai.Acai;
import com.google.common.collect.ImmutableMap;
import com.google.inject.Inject;
import com.google.protobuf.Message;
import com.google.scp.operator.frontend.service.aws.testing.FrontendIntegrationTestEnv;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestProto.CreateJobRequest;
import com.google.scp.operator.protos.shared.backend.metadatadb.JobMetadataProto.JobMetadata;
import com.google.scp.operator.shared.dao.metadatadb.aws.DynamoMetadataDb;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobKeyExistsException;
import com.google.scp.operator.shared.dao.metadatadb.common.JobMetadataDb.JobMetadataDbException;
import com.google.scp.operator.shared.dao.metadatadb.testing.JobGenerator;
import com.google.scp.operator.shared.testing.APIGatewayProxyRequestEventFakeFactory;
import com.google.scp.shared.mapper.TimeObjectMapper;
import com.google.scp.shared.testutils.aws.LambdaResponsePayload;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.core.SdkBytes;
import software.amazon.awssdk.services.lambda.LambdaClient;
import software.amazon.awssdk.services.lambda.model.InvokeRequest;
import software.amazon.awssdk.services.lambda.model.InvokeResponse;

@RunWith(JUnit4.class)
public final class CreateJobIntegrationTest {

  @Rule public Acai acai = new Acai(FrontendIntegrationTestEnv.class);
  private final ObjectMapper mapper = new TimeObjectMapper();
  @Inject private LambdaClient lambdaClient;
  @Inject private DynamoMetadataDb metadataDb;

  @Test(timeout = 120_000)
  public void createJob_success() throws JsonProcessingException, JobMetadataDbException {
    var jobKeyString = "1";
    var inputDataBlobPrefix = "IDBP";
    var inputDataBlobBucket = "IDBB";
    var outputDataBlobPrefix = "ODBP";
    var outputDataBlobBucket = "ODBB";
    var postbackUrl = "foo.com";
    var attributionReportTo = "ART";
    var request =
        CreateJobRequest.newBuilder()
            .setJobRequestId(jobKeyString)
            .setInputDataBlobPrefix(inputDataBlobPrefix)
            .setInputDataBucketName(inputDataBlobBucket)
            .setOutputDataBlobPrefix(outputDataBlobPrefix)
            .setOutputDataBucketName(outputDataBlobBucket)
            .setPostbackUrl(postbackUrl)
            .putAllJobParameters(
                ImmutableMap.of(JOB_PARAM_ATTRIBUTION_REPORT_TO, attributionReportTo))
            .build();

    LambdaResponsePayload payload = invokeAndValidateLambdaResponse(request);

    var jobMetadata = metadataDb.getJobMetadata(jobKeyString).get();
    assertThat(jobMetadata.getJobKey().getJobRequestId()).isEqualTo(jobKeyString);
    assertThat(jobMetadata.getRequestInfo().getInputDataBlobPrefix())
        .isEqualTo(inputDataBlobPrefix);
    assertThat(jobMetadata.getRequestInfo().getInputDataBucketName())
        .isEqualTo(inputDataBlobBucket);
    assertThat(jobMetadata.getRequestInfo().getOutputDataBlobPrefix())
        .isEqualTo(outputDataBlobPrefix);
    assertThat(jobMetadata.getRequestInfo().getOutputDataBucketName())
        .isEqualTo(outputDataBlobBucket);
    assertThat(jobMetadata.getRequestInfo().getPostbackUrl()).isEqualTo(postbackUrl);
    assertThat(jobMetadata.getRequestInfo().getJobParameters()).hasSize(1);
    assertThat(jobMetadata.getRequestInfo().getJobParameters().get(JOB_PARAM_ATTRIBUTION_REPORT_TO))
        .isEqualTo(attributionReportTo);
    assertThat(payload).isNotNull();
    assertThat(payload.headers().get("content-type")).isEqualTo("application/json");
    assertThat(payload.statusCode()).isEqualTo(202);
    assertThat(payload.body()).isNotNull();
  }

  @Test(timeout = 120_000)
  public void createJob_conflict()
      throws JsonProcessingException, JobMetadataDbException, JobKeyExistsException {
    var jobKeyString = "1";
    JobMetadata originalMetadata =
        JobGenerator.createFakeJobMetadata(jobKeyString).toBuilder().clearRecordVersion().build();
    metadataDb.insertJobMetadata(originalMetadata);
    var request =
        CreateJobRequest.newBuilder()
            .setJobRequestId(jobKeyString)
            .setInputDataBlobPrefix("Test input blob prefix")
            .setInputDataBucketName("Test input bucket name")
            .setOutputDataBlobPrefix("Test output blob prefix")
            .setOutputDataBucketName("Test output bucket name")
            .putAllJobParameters(ImmutableMap.of("Test key", "Test value"))
            .build();

    LambdaResponsePayload payload = invokeAndValidateLambdaResponse(request);

    var jobMetadata = metadataDb.getJobMetadata(jobKeyString);
    assertThat(jobMetadata.isPresent()).isTrue();
    assertThat(payload).isNotNull();
    assertThat(payload.headers().get("content-type")).isEqualTo("application/json");
    assertThat(payload.statusCode()).isEqualTo(409);
    assertThat(payload.body()).isNotNull();
  }

  private LambdaResponsePayload invokeAndValidateLambdaResponse(Message proto)
      throws JsonProcessingException {
    APIGatewayProxyRequestEvent requestEvent =
        APIGatewayProxyRequestEventFakeFactory.createFromProtoPost(proto);

    InvokeResponse response =
        lambdaClient.invoke(
            InvokeRequest.builder()
                .functionName(CREATE_JOB_LAMBDA_NAME)
                .payload(SdkBytes.fromUtf8String(mapper.writeValueAsString(requestEvent)))
                .build());

    String responseBody = response.payload().asUtf8String();
    LambdaResponsePayload payload = mapper.readValue(responseBody, LambdaResponsePayload.class);

    return payload;
  }
}
