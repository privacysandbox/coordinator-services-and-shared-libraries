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

package com.google.scp.operator.cpio.metricclient.gcp;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.api.MetricDescriptor;
import com.google.cloud.monitoring.v3.MetricServiceClient;
import com.google.scp.operator.cpio.metricclient.model.CustomMetric;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import java.io.IOException;
import java.util.Optional;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.Mockito;

public class GcpMetricClientManualTest {
  // Change PROJECT_ID, ZONE, INSTANCE_ID to your own testing params.
  private static final String PROJECT_ID = "";
  private static final String ZONE = "us-central1-c";
  private static final String INSTANCE_ID = "";

  // Make your own metric or leave with default value
  private static final String METRIC_NAME = "test metric avoid duplicate";
  private static final String METRIC_NAMESPACE = "scp/metric/test";
  private static final String METRIC_UNIT = "DOUBLE";
  private static final double METRIC_VALUE = 1.6;
  private static final String ENV_NAME = "test-env";

  MetricServiceClient msClient;

  GcpMetricClient metricClient;

  @Mock ParameterClient parameterClient;

  CustomMetric metric;

  @Before
  public void setUp() throws IOException, ParameterClientException {
    parameterClient = Mockito.mock((ParameterClient.class));
    msClient = MetricServiceClient.create();
    when(parameterClient.getEnvironmentName()).thenReturn(Optional.of(ENV_NAME));
    metricClient = new GcpMetricClient(msClient, parameterClient, PROJECT_ID, INSTANCE_ID, ZONE);
    metric =
        CustomMetric.builder()
            .setName(METRIC_NAME)
            .setNameSpace(METRIC_NAMESPACE)
            .setUnit(METRIC_UNIT)
            .setValue(METRIC_VALUE)
            .build();
  }

  @Test
  public void testRecordMetric_Success() throws Exception {
    // act
    metricClient.recordMetric(metric);
    MetricDescriptor metricDescriptor = msClient.getMetricDescriptor(getMetricDescriptorName());
    // assert
    assertThat(metricDescriptor.getType()).isEqualTo(getMetricType());
    // verify
    verify(parameterClient, times(1)).getEnvironmentName();
  }

  private String getMetricDescriptorName() {
    return String.format("projects/%s/metricDescriptors/%s", PROJECT_ID, getMetricType());
  }

  private String getMetricType() {
    return String.format(
        "custom.googleapis.com/%s/%s/%s",
        metric.nameSpace(), ENV_NAME, metric.name().replace(' ', '_').toLowerCase());
  }

  @After
  public void cleanUp() {
    msClient.deleteMetricDescriptor(getMetricDescriptorName());
  }
}
