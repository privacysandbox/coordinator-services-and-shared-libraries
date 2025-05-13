/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.shared.otel;

import com.google.common.collect.ImmutableList;

public class OTelSemConMetrics {
  private OTelSemConMetrics() {}

  public static final String HTTP_SERVER_REQUEST_DURATION = "http.server.request.duration";

  private static final double LATENCY_HISTOGRAM_BOUNDARIES_POWER_BASE = Math.pow(10.0, 0.1);
  private static final double LATENCY_HISTOGRAM_BOUNDARIES_SCALE_FACTOR = 0.0001; // 100µs
  private static final int LATENCY_HISTOGRAM_BOUNDARIES_NUM_BUCKETS = 76; // 1.1h

  private static ImmutableList<Double> makeLatencyHistogramBoundaries() {
    ImmutableList.Builder<Double> boundaries = ImmutableList.builder();
    for (int i = 0; i <= LATENCY_HISTOGRAM_BOUNDARIES_NUM_BUCKETS; i++) {
      double boundary =
          Math.pow(LATENCY_HISTOGRAM_BOUNDARIES_POWER_BASE, i)
              * LATENCY_HISTOGRAM_BOUNDARIES_SCALE_FACTOR;
      boundaries.add(boundary);
    }
    return boundaries.build();
  }

  public static final ImmutableList<Double> LATENCY_HISTOGRAM_BOUNDARIES =
      makeLatencyHistogramBoundaries();
}
