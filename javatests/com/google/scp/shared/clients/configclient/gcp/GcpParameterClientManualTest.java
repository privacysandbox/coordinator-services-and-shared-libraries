package com.google.scp.shared.clients.configclient.gcp;

import static com.google.common.truth.Truth8.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.cloud.secretmanager.v1.SecretManagerServiceClient;
import com.google.common.truth.Truth;
import com.google.scp.shared.clients.configclient.model.WorkerParameter;
import java.io.IOException;
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
public final class GcpParameterClientManualTest {

  private static final String PROJECT_ID = "adh-cloud-adtech1";
  private static final String ENVIRONMENT = "manual_test_environment";
  private static final String JOB_QUEUE_VALUE = "queue1";
  private static final String JOB_METADATA_DB_VALUE = "test-job-metadata-db";

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Mock private GcpMetadataServiceClient metadataServiceClient;

  private GcpParameterClient parameterClient;

  @Before
  public void setUp() throws IOException {
    SecretManagerServiceClientProxy proxy =
        new SecretManagerServiceClientProxy(SecretManagerServiceClient.create());
    parameterClient = new GcpParameterClient(proxy, metadataServiceClient, PROJECT_ID);
  }

  @Test
  public void getParameter_withNoPrefixNoEnvironment() throws Exception {
    when(metadataServiceClient.getMetadata(any())).thenReturn(Optional.of(ENVIRONMENT));

    Optional<String> val =
        parameterClient.getParameter(
            WorkerParameter.JOB_QUEUE.name(),
            Optional.empty(),
            /* includeEnvironmentParam= */ false);

    assertThat(val).isPresent();
    Truth.assertThat(val.get()).isEqualTo(JOB_QUEUE_VALUE);
    verify(metadataServiceClient, never()).getMetadata(any());
  }

  @Test
  public void getParameter_returnsParameter() throws Exception {
    when(metadataServiceClient.getMetadata(any())).thenReturn(Optional.of(ENVIRONMENT));

    Optional<String> val = parameterClient.getParameter(WorkerParameter.JOB_METADATA_DB.name());

    assertThat(val).isPresent();
    Truth.assertThat(val.get()).isEqualTo(JOB_METADATA_DB_VALUE);
    verify(metadataServiceClient).getMetadata(eq("scp-environment"));
  }

  @Test
  public void getParameter_nonexistentParameter() throws Exception {
    when(metadataServiceClient.getMetadata(any())).thenReturn(Optional.of(ENVIRONMENT));

    Optional<String> val = parameterClient.getParameter("test-nonexistent");
    assertThat(val).isEmpty();
    verify(metadataServiceClient).getMetadata(eq("scp-environment"));
  }
}
