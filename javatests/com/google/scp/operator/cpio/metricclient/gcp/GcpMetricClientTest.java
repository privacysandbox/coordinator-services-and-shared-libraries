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

import com.google.cloud.monitoring.v3.MetricServiceClient;
import com.google.scp.operator.cpio.metricclient.model.CustomMetric;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import java.io.IOException;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public class GcpMetricClientTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  private GcpMetricClient metricClient;

  @Mock private ParameterClient parameterClient;

  @Before
  public void setUp() throws IOException {
    MetricServiceClient msClient = MetricServiceClient.create();
    metricClient =
        new GcpMetricClient(
            msClient, parameterClient, "testProject123", "testInstance123", "testZone123");
  }

  @Test
  public void testRecordMetric_ApiCallSuccessButThrowExceptionDueToFakeCall()
      throws ParameterClientException {
    // arrange
    CustomMetric metric =
        CustomMetric.builder()
            .setName("testMetric")
            .setNameSpace("scp/test")
            .setUnit("Double")
            .setValue(1.2)
            .build();
    // act
    try {
      metricClient.recordMetric(metric);
    } catch (Exception e) {
      // assert
      assertThat(e.getMessage()).contains("io.grpc.StatusRuntimeException");
      // verify
      verify(parameterClient, times(1)).getEnvironmentName();
    }
  }
}
