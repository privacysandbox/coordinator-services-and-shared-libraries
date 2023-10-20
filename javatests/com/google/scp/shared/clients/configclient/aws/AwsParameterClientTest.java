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

package com.google.scp.shared.clients.configclient.aws;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.scp.shared.clients.configclient.model.WorkerParameter;
import java.util.Optional;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import software.amazon.awssdk.services.ec2.Ec2Client;
import software.amazon.awssdk.services.ec2.model.DescribeTagsRequest;
import software.amazon.awssdk.services.ec2.model.DescribeTagsResponse;
import software.amazon.awssdk.services.ec2.model.TagDescription;
import software.amazon.awssdk.services.ssm.SsmClient;
import software.amazon.awssdk.services.ssm.model.GetParametersRequest;
import software.amazon.awssdk.services.ssm.model.GetParametersResponse;
import software.amazon.awssdk.services.ssm.model.Parameter;

@RunWith(JUnit4.class)
public class AwsParameterClientTest {

  private static final String ENVIRONMENT_TAG_NAME = "environment";
  private static final String ENVIRONMENT = "test";
  private static final String QUEUE_URL = "https://test_queue";
  private static final String DDB_TABLE_NAME = "test_table";
  private static final String MAX_JOB_NUM_ATTEMPTS = "test_max_job_num_attempts";
  private static final String MAX_JOB_PROCESSING_TIME_SECONDS = "test_max_job_processing_time_secs";
  private static final String COORDINATOR_A_ROLE = "test_coordinator_a_role";
  private static final String COORDINATOR_B_ROLE = "test_coordinator_b_role";
  private static final String COORDINATOR_KMS_ARN = "test_party_kms_arn";

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private SsmClient ssmClient;
  @Mock private Ec2Client ec2Client;
  private AwsParameterClient parameterClient;

  @Before
  public void setUp() {
    // We are using mocked ec2Client and ssmClient instance, which is not straightforward
    // to inject in Guice. So we do not use module injection for these clients in this test,
    // and call AwsParameterClient constructor directly for instantiation.
    parameterClient = new AwsParameterClient(ec2Client, ssmClient);
  }

  @Test
  public void getParameter_withSingleParameter() throws Exception {
    var fakeSsmResponse =
        GetParametersResponse.builder()
            .parameters(
                Parameter.builder()
                    .name("scp-" + ENVIRONMENT + "-test")
                    .value("test-value")
                    .build())
            .build();
    var fakeEc2Response =
        DescribeTagsResponse.builder()
            .tags(TagDescription.builder().key(ENVIRONMENT_TAG_NAME).value(ENVIRONMENT).build())
            .build();
    when(ssmClient.getParameters(any(GetParametersRequest.class))).thenReturn(fakeSsmResponse);
    when(ec2Client.describeTags(any(DescribeTagsRequest.class))).thenReturn(fakeEc2Response);

    Optional<String> actual = parameterClient.getParameter("test");

    verify(ec2Client).describeTags(any(DescribeTagsRequest.class));
    verify(ssmClient).getParameters(any(GetParametersRequest.class));
    assertThat(actual).isPresent();
    assertThat(actual.get()).isEqualTo("test-value");
  }

  @Test
  public void getParameter_withCustomPrefixAndEnvironment() throws Exception {
    var fakeSsmResponse =
        GetParametersResponse.builder()
            .parameters(
                Parameter.builder()
                    .name("prefix-" + ENVIRONMENT + "-param")
                    .value("test-value")
                    .build())
            .build();
    var fakeEc2Response =
        DescribeTagsResponse.builder()
            .tags(TagDescription.builder().key(ENVIRONMENT_TAG_NAME).value(ENVIRONMENT).build())
            .build();
    when(ssmClient.getParameters(any(GetParametersRequest.class))).thenReturn(fakeSsmResponse);
    when(ec2Client.describeTags(any(DescribeTagsRequest.class))).thenReturn(fakeEc2Response);

    Optional<String> actual =
        parameterClient.getParameter(
            "param", Optional.of("prefix"), /* includeEnvironmentParam= */ true);

    verify(ec2Client).describeTags(any(DescribeTagsRequest.class));
    verify(ssmClient).getParameters(any(GetParametersRequest.class));
    assertThat(actual).isPresent();
    assertThat(actual.get()).isEqualTo("test-value");
  }

  @Test
  public void getParameter_withCustomPrefixNoEnvironment() throws Exception {
    var fakeSsmResponse =
        GetParametersResponse.builder()
            .parameters(Parameter.builder().name("prefix-param").value("test-value").build())
            .build();
    var fakeEc2Response =
        DescribeTagsResponse.builder()
            .tags(TagDescription.builder().key(ENVIRONMENT_TAG_NAME).value(ENVIRONMENT).build())
            .build();
    when(ssmClient.getParameters(any(GetParametersRequest.class))).thenReturn(fakeSsmResponse);
    when(ec2Client.describeTags(any(DescribeTagsRequest.class))).thenReturn(fakeEc2Response);

    Optional<String> actual =
        parameterClient.getParameter(
            "param", Optional.of("prefix"), /* includeEnvironmentParam= */ false);

    verify(ec2Client, never()).describeTags(any(DescribeTagsRequest.class));
    verify(ssmClient).getParameters(any(GetParametersRequest.class));
    assertThat(actual).isPresent();
    assertThat(actual.get()).isEqualTo("test-value");
  }

  @Test
  public void getParameter_withEnvironmentNoPrefix() throws Exception {
    var fakeSsmResponse =
        GetParametersResponse.builder()
            .parameters(
                Parameter.builder().name(ENVIRONMENT + "-param").value("test-value").build())
            .build();
    var fakeEc2Response =
        DescribeTagsResponse.builder()
            .tags(TagDescription.builder().key(ENVIRONMENT_TAG_NAME).value(ENVIRONMENT).build())
            .build();
    when(ssmClient.getParameters(any(GetParametersRequest.class))).thenReturn(fakeSsmResponse);
    when(ec2Client.describeTags(any(DescribeTagsRequest.class))).thenReturn(fakeEc2Response);

    Optional<String> actual =
        parameterClient.getParameter(
            "param", Optional.empty(), /* includeEnvironmentParam= */ true);

    verify(ec2Client).describeTags(any(DescribeTagsRequest.class));
    verify(ssmClient).getParameters(any(GetParametersRequest.class));
    assertThat(actual).isPresent();
    assertThat(actual.get()).isEqualTo("test-value");
  }

  @Test
  public void getParameter_withNoPrefixNoEnvironment() throws Exception {
    var fakeSsmResponse =
        GetParametersResponse.builder()
            .parameters(Parameter.builder().name("param").value("test-value").build())
            .build();
    var fakeEc2Response =
        DescribeTagsResponse.builder()
            .tags(TagDescription.builder().key(ENVIRONMENT_TAG_NAME).value(ENVIRONMENT).build())
            .build();
    when(ssmClient.getParameters(any(GetParametersRequest.class))).thenReturn(fakeSsmResponse);
    when(ec2Client.describeTags(any(DescribeTagsRequest.class))).thenReturn(fakeEc2Response);

    Optional<String> actual =
        parameterClient.getParameter(
            "param", Optional.empty(), /* includeEnvironmentParam= */ false);

    verify(ec2Client, never()).describeTags(any(DescribeTagsRequest.class));
    verify(ssmClient).getParameters(any(GetParametersRequest.class));
    assertThat(actual).isPresent();
    assertThat(actual.get()).isEqualTo("test-value");
  }

  @Test
  public void getParameter_withNonexistentSingleParameter() throws Exception {
    var fakeSsmResponse = GetParametersResponse.builder().build();
    var fakeEc2Response =
        DescribeTagsResponse.builder()
            .tags(TagDescription.builder().key(ENVIRONMENT_TAG_NAME).value(ENVIRONMENT).build())
            .build();
    when(ssmClient.getParameters(any(GetParametersRequest.class))).thenReturn(fakeSsmResponse);
    when(ec2Client.describeTags(any(DescribeTagsRequest.class))).thenReturn(fakeEc2Response);

    Optional<String> actual = parameterClient.getParameter("test");

    verify(ec2Client).describeTags(any(DescribeTagsRequest.class));
    verify(ssmClient).getParameters(any(GetParametersRequest.class));
    assertThat(actual).isEmpty();
  }

  @Test
  public void getParameter_throwsEc2EnvironmentTagNotSet() throws Exception {
    var fakeEc2Response = DescribeTagsResponse.builder().build();
    when(ec2Client.describeTags(any(DescribeTagsRequest.class))).thenReturn(fakeEc2Response);

    assertThrows(
        IllegalStateException.class,
        () -> parameterClient.getParameter(WorkerParameter.JOB_QUEUE.name()));

    verify(ec2Client).describeTags(any(DescribeTagsRequest.class));
  }

  @Test
  public void getParameter_invalidParams() throws Exception {
    var fakeSsmResponse =
        GetParametersResponse.builder()
            .invalidParameters("scp-" + ENVIRONMENT + "-SCALE_IN_HOOK")
            .build();
    var fakeEc2Response =
        DescribeTagsResponse.builder()
            .tags(TagDescription.builder().key(ENVIRONMENT_TAG_NAME).value(ENVIRONMENT).build())
            .build();
    when(ec2Client.describeTags(any(DescribeTagsRequest.class))).thenReturn(fakeEc2Response);
    when(ssmClient.getParameters(any(GetParametersRequest.class))).thenReturn(fakeSsmResponse);

    Optional<String> result = parameterClient.getParameter(WorkerParameter.SCALE_IN_HOOK.name());

    assertThat(result).isEmpty();
    verify(ec2Client).describeTags(any(DescribeTagsRequest.class));
    verify(ssmClient).getParameters(any(GetParametersRequest.class));
  }
}
