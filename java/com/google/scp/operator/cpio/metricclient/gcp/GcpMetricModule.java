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

import com.google.cloud.monitoring.v3.MetricServiceClient;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.metricclient.MetricClient;
import com.google.scp.operator.cpio.metricclient.MetricModule;
import java.io.IOException;

@Singleton
public class GcpMetricModule extends MetricModule {

  @Override
  public Class<? extends MetricClient> getMetricClientImpl() {
    return GcpMetricClient.class;
  }

  @Provides
  @Singleton
  MetricServiceClient provideMetricServiceClient() throws IOException {
    return MetricServiceClient.create();
  }
}
