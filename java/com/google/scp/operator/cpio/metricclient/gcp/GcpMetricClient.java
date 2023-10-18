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

import com.google.api.Metric;
import com.google.api.MetricDescriptor.MetricKind;
import com.google.api.MetricDescriptor.ValueType;
import com.google.api.MonitoredResource;
import com.google.api.gax.rpc.ApiException;
import com.google.cloud.monitoring.v3.MetricServiceClient;
import com.google.monitoring.v3.CreateTimeSeriesRequest;
import com.google.monitoring.v3.Point;
import com.google.monitoring.v3.ProjectName;
import com.google.monitoring.v3.TimeInterval;
import com.google.monitoring.v3.TimeSeries;
import com.google.monitoring.v3.TypedValue;
import com.google.protobuf.util.Timestamps;
import com.google.scp.operator.cpio.metricclient.MetricClient;
import com.google.scp.operator.cpio.metricclient.model.CustomMetric;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpInstanceId;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpProjectId;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpZone;
import io.grpc.StatusRuntimeException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.logging.Logger;
import javax.inject.Inject;

/** {@code MetricClient} implementation for GCP */
public final class GcpMetricClient implements MetricClient {

  private static final Logger logger = Logger.getLogger(GcpMetricClient.class.getName());
  private static final String DEFAULT_ENV = "dms-default-env";
  private final String instanceId;
  private final String zone;
  private final String projectId;
  private final ParameterClient parameterClient;
  private final MetricServiceClient msClient;

  @Inject
  GcpMetricClient(
      MetricServiceClient msClient,
      ParameterClient parameterClient,
      @GcpProjectId String projectId,
      @GcpInstanceId String instanceId,
      @GcpZone String zone) {
    this.msClient = msClient;
    this.parameterClient = parameterClient;
    this.instanceId = instanceId;
    this.zone = zone;
    this.projectId = projectId;
  }

  @Override
  public void recordMetric(CustomMetric metric) throws MetricClientException {
    try {
      ProjectName projectName = ProjectName.of(projectId);
      TimeInterval interval =
          TimeInterval.newBuilder()
              .setEndTime(Timestamps.fromMillis(System.currentTimeMillis()))
              .build();
      TypedValue value = TypedValue.newBuilder().setDoubleValue(metric.value()).build();
      Point point = Point.newBuilder().setInterval(interval).setValue(value).build();

      List<Point> pointList = new ArrayList<>();
      pointList.add(point);

      Map<String, String> metricLabels = new HashMap<String, String>(metric.labels());
      String metricType =
          String.format(
              "custom.googleapis.com/%s/%s/%s",
              metric.nameSpace(),
              this.getEnvironmentName(),
              metric.name().replace(' ', '_').toLowerCase());
      Metric gcpMetric = Metric.newBuilder().setType(metricType).putAllLabels(metricLabels).build();
      Map<String, String> resourceLabels = new HashMap<>();
      resourceLabels.put("instance_id", instanceId);
      resourceLabels.put("zone", zone);
      resourceLabels.put("project_id", projectId);

      MonitoredResource resource =
          MonitoredResource.newBuilder()
              .setType("gce_instance")
              .putAllLabels(resourceLabels)
              .build();

      // Prepares the time series request
      TimeSeries timeSeries =
          TimeSeries.newBuilder()
              .setMetric(gcpMetric)
              .setResource(resource)
              .addAllPoints(pointList)
              .setValueType(ValueType.DOUBLE)
              .setMetricKind(MetricKind.GAUGE)
              .build();

      List<TimeSeries> timeSeriesList = new ArrayList<>();
      timeSeriesList.add(timeSeries);

      // Writes time series data
      CreateTimeSeriesRequest request =
          CreateTimeSeriesRequest.newBuilder()
              .setName(projectName.toString())
              .addAllTimeSeries(timeSeriesList)
              .build();

      // write metric
      msClient.createTimeSeries(request);
    } catch (ApiException | StatusRuntimeException e) {
      throw new MetricClientException(e);
    }
  }

  private String getEnvironmentName() {
    Optional<String> environment = Optional.empty();
    try {
      environment = parameterClient.getEnvironmentName();
    } catch (ParameterClientException e) {
      logger.info(String.format("Could not get environment name.\n%s", e));
    }
    if (environment.isEmpty()) {
      logger.warning(
          String.format(
              "Defaulting to environment name %s for custom monitoring metrics.", DEFAULT_ENV));
    }
    return environment.orElse(DEFAULT_ENV);
  }
}
