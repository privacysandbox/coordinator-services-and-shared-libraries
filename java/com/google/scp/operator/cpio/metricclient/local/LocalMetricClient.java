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

package com.google.scp.operator.cpio.metricclient.local;

import com.google.scp.operator.cpio.metricclient.MetricClient;
import com.google.scp.operator.cpio.metricclient.model.CustomMetric;
import java.util.Optional;
import java.util.logging.Logger;

/** {@code MetricClient} implementation for local testing */
public final class LocalMetricClient implements MetricClient {

  private static final Logger logger = Logger.getLogger(LocalMetricClient.class.getName());

  /** The last metric that was recorded. */
  public Optional<CustomMetric> lastReceivedMetric = Optional.empty();

  @Override
  public void recordMetric(CustomMetric metric) {
    // Print out for convenience.
    System.out.println(metric);
    lastReceivedMetric = Optional.of(metric);
  }
}
