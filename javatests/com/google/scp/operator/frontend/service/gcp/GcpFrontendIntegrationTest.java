package com.google.scp.operator.frontend.service.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.operator.frontend.service.model.Constants.JOB_PARAM_ATTRIBUTION_REPORT_TO;
import static com.google.scp.operator.frontend.service.model.Constants.JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT;
import static com.google.scp.shared.api.model.Code.ACCEPTED;
import static com.google.scp.shared.api.model.Code.ALREADY_EXISTS;
import static com.google.scp.shared.api.model.Code.NOT_FOUND;
import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.testutils.common.HttpRequestUtil.executeRequestWithRetry;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.acai.Acai;
import com.google.common.collect.ImmutableMap;
import com.google.inject.Inject;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.operator.frontend.service.gcp.testing.FrontendServiceIntegrationTestEnv;
import com.google.scp.operator.protos.frontend.api.v1.CreateJobRequestProto.CreateJobRequest;
import com.google.scp.operator.protos.shared.backend.jobqueue.JobQueueProto.JobQueueItem;
import com.google.scp.operator.shared.dao.jobqueue.common.JobQueue.JobQueueException;
import com.google.scp.operator.shared.dao.jobqueue.gcp.PubSubJobQueue;
import com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer;
import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.util.Optional;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Frontend integration test. Tests createJob and getJob requests. */
@RunWith(JUnit4.class)
public final class GcpFrontendIntegrationTest {
  @Rule public Acai acai = new Acai(FrontendServiceIntegrationTestEnv.class);
  @Inject PubSubJobQueue pubSubJobQueue;
  @Inject private CloudFunctionEmulatorContainer functionContainer;

  private static final HttpClient client = HttpClient.newHttpClient();
  private static final String createJobPath = "/v1alpha/createJob";
  private static final String getJobPath = "/v1alpha/getJob";
  private static CreateJobRequest createJobRequest;

  @Before
  public void setUp() {
    createJobRequest =
        CreateJobRequest.newBuilder()
            .setJobRequestId("123")
            .setInputDataBlobPrefix("test")
            .setInputDataBucketName("inputBucket")
            .setOutputDataBlobPrefix("test")
            .setOutputDataBucketName("outputBucket")
            .setPostbackUrl("http://postback.com")
            .putAllJobParameters(
                ImmutableMap.of(
                    JOB_PARAM_ATTRIBUTION_REPORT_TO,
                    "foo.com",
                    JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT,
                    "5"))
            .build();
  }

  @Test
  public void testCreateAndGetJob_success() throws IOException, JobQueueException {
    HttpRequest createRequest =
        HttpRequest.newBuilder()
            .uri(getFunctionUri(createJobPath))
            .setHeader("content-type", "application/json")
            .POST(HttpRequest.BodyPublishers.ofString(getCreateRequestJson()))
            .build();
    HttpResponse<String> createJobResponse = executeRequestWithRetry(client, createRequest);
    Optional<JobQueueItem> item = pubSubJobQueue.receiveJob();

    HttpRequest getRequest =
        HttpRequest.newBuilder()
            .uri(
                getFunctionUri(
                    String.format(
                        "%s?job_request_id=%s", getJobPath, createJobRequest.getJobRequestId())))
            .setHeader("content-type", "application/json")
            .GET()
            .build();
    HttpResponse<String> getJobResponse = executeRequestWithRetry(client, getRequest);
    JsonNode getJobNode = new ObjectMapper().readTree(getJobResponse.body());

    assertThat(createJobResponse.statusCode()).isEqualTo(ACCEPTED.getHttpStatusCode());
    assertThat(getJobResponse.statusCode()).isEqualTo(OK.getHttpStatusCode());
    assertThat(item.isPresent()).isTrue();
    assertThat(item.get().getJobKeyString()).isEqualTo(createJobRequest.getJobRequestId());
    assertThat(
            createJobResponse
                .headers()
                .map()
                .get("content-type")
                .get(0)
                .contains("application/json"))
        .isTrue();
    assertThat(
            getJobResponse.headers().map().get("content-type").get(0).contains("application/json"))
        .isTrue();
    assertThat(getJobNode.get("job_request_id").asText())
        .isEqualTo(createJobRequest.getJobRequestId());
    assertThat(getJobNode.get("job_status").asText()).isEqualTo("RECEIVED");
    assertThat(getJobNode.get("input_data_blob_prefix").asText())
        .isEqualTo(createJobRequest.getInputDataBlobPrefix());
    assertThat(getJobNode.get("output_data_blob_prefix").asText())
        .isEqualTo(createJobRequest.getOutputDataBlobPrefix());
    assertThat(getJobNode.get("input_data_bucket_name").asText())
        .isEqualTo(createJobRequest.getInputDataBucketName());
    assertThat(getJobNode.get("output_data_bucket_name").asText())
        .isEqualTo(createJobRequest.getOutputDataBucketName());
    assertThat(getJobNode.get("postback_url").asText())
        .isEqualTo(createJobRequest.getPostbackUrl());
    assertThat(getJobNode.get("job_parameters").get(JOB_PARAM_ATTRIBUTION_REPORT_TO).asText())
        .isEqualTo(createJobRequest.getJobParametersOrThrow(JOB_PARAM_ATTRIBUTION_REPORT_TO));
    assertThat(getJobNode.get("job_parameters").get(JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT).asText())
        .isEqualTo(createJobRequest.getJobParametersOrThrow(JOB_PARAM_DEBUG_PRIVACY_BUDGET_LIMIT));
  }

  private String getCreateRequestJson() throws InvalidProtocolBufferException {
    return JsonFormat.printer().print(createJobRequest);
  }

  @Test
  public void testCreateJob_duplicateId() throws IOException, InterruptedException {
    HttpRequest createRequest =
        HttpRequest.newBuilder()
            .uri(getFunctionUri(createJobPath))
            .setHeader("Content-type", "application/json")
            .POST(HttpRequest.BodyPublishers.ofString(getCreateRequestJson()))
            .build();
    HttpResponse<String> createJobResponse =
        client.send(createRequest, HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));

    assertThat(createJobResponse.statusCode()).isEqualTo(ALREADY_EXISTS.getHttpStatusCode());
    assertThat(createJobResponse.body().contains("DUPLICATE_JOB_KEY")).isTrue();
  }

  @Test
  public void testGetJob_notFound() {
    HttpRequest getRequest =
        HttpRequest.newBuilder()
            .uri(getFunctionUri(String.format("%s?job_request_id=%s", getJobPath, "321")))
            .GET()
            .build();
    HttpResponse<String> getJobResponse = executeRequestWithRetry(client, getRequest);
    assertThat(getJobResponse.statusCode()).isEqualTo(NOT_FOUND.getHttpStatusCode());
    assertThat(getJobResponse.body().contains("JOB_NOT_FOUND")).isTrue();
  }

  private URI getFunctionUri(String path) {
    return URI.create("http://" + functionContainer.getEmulatorEndpoint() + path);
  }
}
