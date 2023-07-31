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

package com.google.scp.operator.cpio.metricclient.aws;

import com.google.common.collect.ImmutableList;
import com.google.scp.operator.cpio.metricclient.MetricClient;
import com.google.scp.operator.cpio.metricclient.MetricClient.MetricClientException;
import com.google.scp.operator.cpio.metricclient.model.CustomMetric;
import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import java.time.Clock;
import java.time.Instant;
import java.util.Optional;
import java.util.logging.Logger;
import javax.inject.Inject;
import software.amazon.awssdk.services.cloudwatch.CloudWatchClient;
import software.amazon.awssdk.services.cloudwatch.model.CloudWatchException;
import software.amazon.awssdk.services.cloudwatch.model.Dimension;
import software.amazon.awssdk.services.cloudwatch.model.MetricDatum;
import software.amazon.awssdk.services.cloudwatch.model.PutMetricDataRequest;
import software.amazon.awssdk.services.cloudwatch.model.StandardUnit;

/** {@code MetricClient} implementation for AWS cloudwatch */
public final class AwsMetricClient implements MetricClient {

  private static final Logger logger = Logger.getLogger(AwsMetricClient.class.getName());
  private static final String DEFAULT_ENV = "dms-default-env";

  private final CloudWatchClient cwClient;
  private final ParameterClient parameterClient;
  private final Clock clock;

  /** Creates a new instance of the {@code AwsMetricClient} class. */
  @Inject
  AwsMetricClient(CloudWatchClient cwClient, ParameterClient parameterClient, Clock clock) {
    this.cwClient = cwClient;
    this.parameterClient = parameterClient;
    this.clock = clock;
  }

  @Override
  public void recordMetric(CustomMetric metric) throws MetricClientException {
    try {
      MetricDatum datum =
          MetricDatum.builder()
              .metricName(metric.name())
              .unit(StandardUnit.fromValue(metric.unit()))
              .value(metric.value())
              .dimensions(
                  metric.labels().entrySet().stream()
                      .map(
                          label ->
                              Dimension.builder()
                                  .name(label.getKey())
                                  .value(label.getValue())
                                  .build())
                      .collect(ImmutableList.toImmutableList()))
              .timestamp(Instant.now(clock))
              .build();

      PutMetricDataRequest request =
          PutMetricDataRequest.builder()
              .namespace(String.format("%s/%s", getEnvironmentName(), metric.nameSpace()))
              .metricData(datum)
              .build();

      cwClient.putMetricData(request);

    } catch (CloudWatchException e) {
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
