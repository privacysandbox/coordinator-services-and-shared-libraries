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

import java.util.Arrays;
import java.util.List;

public class OTelSemConMetrics {
  private OTelSemConMetrics() {}

  public static final String HTTP_SERVER_REQUEST_DURATION = "http.server.request.duration";

  public static final List<Double> LATENCY_BUCKETS =
      Arrays.asList(
          0.005, 0.01, 0.025, 0.05, 0.075, 0.1, 0.25, 0.5, 0.75, 1.0, 2.0, 5.0, 7.5, 10.0);
}
