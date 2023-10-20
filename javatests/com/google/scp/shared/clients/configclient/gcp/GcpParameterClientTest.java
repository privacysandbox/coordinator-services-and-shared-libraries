package com.google.scp.shared.clients.configclient.gcp;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.api.gax.grpc.GrpcStatusCode;
import com.google.api.gax.rpc.NotFoundException;
import com.google.cloud.secretmanager.v1.AccessSecretVersionResponse;
import com.google.cloud.secretmanager.v1.SecretPayload;
import com.google.protobuf.ByteString;
import com.google.scp.shared.clients.configclient.model.WorkerParameter;
import io.grpc.Status;
import io.grpc.Status.Code;
import io.grpc.StatusRuntimeException;
import java.nio.charset.StandardCharsets;
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
public final class GcpParameterClientTest {

  private static final String PROJECT_ID = "projectId1";
  private static final String ENVIRONMENT = "environment";

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private SecretManagerServiceClientProxy secretManagerServiceClientProxy;
  @Mock private GcpMetadataServiceClient metadataServiceClient;

  private GcpParameterClient parameterClient;

  @Before
  public void setUp() {
    parameterClient =
        new GcpParameterClient(secretManagerServiceClientProxy, metadataServiceClient, PROJECT_ID);
  }

  @Test
  public void getParameter_returnsParameter() throws Exception {
    String secretName = getSecretName("scp-environment-JOB_PUBSUB_TOPIC_ID");
    String secretValue = "testVal";
    ByteString data = ByteString.copyFrom(secretValue.getBytes(StandardCharsets.UTF_8));
    when(metadataServiceClient.getMetadata(any())).thenReturn(Optional.of(ENVIRONMENT));
    when(secretManagerServiceClientProxy.accessSecretVersion(secretName))
        .thenReturn(
            AccessSecretVersionResponse.newBuilder()
                .setPayload(SecretPayload.newBuilder().setData(data))
                .build());

    Optional<String> val = parameterClient.getParameter(WorkerParameter.JOB_PUBSUB_TOPIC_ID.name());

    assertThat(val).isPresent();
    assertThat(val.get()).isEqualTo(secretValue);
    verify(metadataServiceClient).getMetadata(eq("scp-environment"));
    verify(secretManagerServiceClientProxy).accessSecretVersion(secretName);
  }

  @Test
  public void getParameter_withPrefixAndEnvironment() throws Exception {
    String secretName = getSecretName("prefix-environment-param");
    String secretValue = "testVal";
    ByteString data = ByteString.copyFrom(secretValue.getBytes(StandardCharsets.UTF_8));
    when(metadataServiceClient.getMetadata(any())).thenReturn(Optional.of(ENVIRONMENT));
    when(secretManagerServiceClientProxy.accessSecretVersion(secretName))
        .thenReturn(
            AccessSecretVersionResponse.newBuilder()
                .setPayload(SecretPayload.newBuilder().setData(data))
                .build());

    Optional<String> val =
        parameterClient.getParameter(
            "param", Optional.of("prefix"), /* includeEnvironmentParam= */ true);

    assertThat(val).isPresent();
    assertThat(val.get()).isEqualTo(secretValue);
    verify(metadataServiceClient).getMetadata(eq("scp-environment"));
    verify(secretManagerServiceClientProxy).accessSecretVersion(secretName);
  }

  @Test
  public void getParameter_withNoPrefixNoEnvironment() throws Exception {
    String secretName = getSecretName("testParam");
    String secretValue = "testVal";
    ByteString data = ByteString.copyFrom(secretValue.getBytes(StandardCharsets.UTF_8));
    when(metadataServiceClient.getMetadata(any())).thenReturn(Optional.of(ENVIRONMENT));
    when(secretManagerServiceClientProxy.accessSecretVersion(secretName))
        .thenReturn(
            AccessSecretVersionResponse.newBuilder()
                .setPayload(SecretPayload.newBuilder().setData(data))
                .build());

    Optional<String> val =
        parameterClient.getParameter(
            "testParam", Optional.empty(), /* includeEnvironmentParam= */ false);

    assertThat(val).isPresent();
    assertThat(val.get()).isEqualTo(secretValue);
    verify(metadataServiceClient, never()).getMetadata(eq("scp-environment"));
    verify(secretManagerServiceClientProxy).accessSecretVersion(secretName);
  }

  @Test
  public void getParameter_notFound() throws Exception {
    String secretName = getSecretName("scp-environment-JOB_QUEUE");
    when(metadataServiceClient.getMetadata(any())).thenReturn(Optional.of(ENVIRONMENT));
    when(secretManagerServiceClientProxy.accessSecretVersion(secretName))
        .thenThrow(
            new NotFoundException(
                new StatusRuntimeException(Status.NOT_FOUND),
                GrpcStatusCode.of(Code.NOT_FOUND),
                /* retryable= */ false));

    Optional<String> val = parameterClient.getParameter(WorkerParameter.JOB_QUEUE.name());

    assertThat(val).isEmpty();
    verify(secretManagerServiceClientProxy).accessSecretVersion(secretName);
  }

  private String getSecretName(String parameter) {
    return String.format("projects/%s/secrets/%s/versions/latest", PROJECT_ID, parameter);
  }
}
