/*
 * Copyright 2024 Google LLC
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
package com.google.scp.shared.otel;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.cloud.opentelemetry.metric.GoogleCloudMetricExporter;
import com.google.cloud.opentelemetry.metric.MetricConfiguration;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import io.opentelemetry.api.OpenTelemetry;
import io.opentelemetry.api.common.Attributes;
import io.opentelemetry.contrib.gcp.resource.GCPResourceProvider;
import io.opentelemetry.sdk.OpenTelemetrySdk;
import io.opentelemetry.sdk.autoconfigure.spi.ConfigProperties;
import io.opentelemetry.sdk.autoconfigure.spi.internal.DefaultConfigProperties;
import io.opentelemetry.sdk.metrics.SdkMeterProvider;
import io.opentelemetry.sdk.metrics.SdkMeterProviderBuilder;
import io.opentelemetry.sdk.metrics.export.MetricExporter;
import io.opentelemetry.sdk.metrics.export.PeriodicMetricReader;
import io.opentelemetry.sdk.resources.Resource;
import java.io.IOException;
import java.util.Collections;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * A module which binds necessary and common classes for initializing all types of {@link
 * OpenTelemetry}.
 */
public class OTelConfigurationModule extends AbstractModule {
  private static final Logger logger = LoggerFactory.getLogger(OTelConfigurationModule.class);

  private static final String PROJECT_ID_ENV_VAR = "PROJECT_ID";
  private static final String EXPORT_OTEL_METRICS = "EXPORT_OTEL_METRICS";

  @Provides
  @Singleton
  OpenTelemetry provideOpenTelemetry() throws IOException {
    // TODO(b/371424019): KeyGeneration service does not run in cloud run functions. It uses a
    // different method for getting variables. Will need to have another way to get this envvar when
    // we instrument keygen metrics.
    String exportOTelMetrics = System.getenv().get(EXPORT_OTEL_METRICS);
    String projectID = System.getenv().get(PROJECT_ID_ENV_VAR);
    if (!exportOTelMetrics.equalsIgnoreCase("true")) {
      logger.info("EXPORT_OTEL_METRICS not set or set to False, using noop OpenTelemetry SDK");
      return OpenTelemetry.noop();
    } else if (projectID == null) {
      logger.warn("Cannot find project id from environment variable. Using noop OpenTelemetry SDK");
      return OpenTelemetry.noop();
    }
    MetricExporter cloudMonitoringExporter =
        GoogleCloudMetricExporter.createWithConfiguration(
            MetricConfiguration.builder()
                .setCredentials(GoogleCredentials.getApplicationDefault())
                .setProjectId(projectID.strip())
                .setDeadline(java.time.Duration.ofSeconds(60))
                .setPrefix("custom.googleapis.com")
                .build());

    ConfigProperties cp = DefaultConfigProperties.createFromMap(Collections.emptyMap());
    Resource resource = new GCPResourceProvider().createResource(cp);
    Attributes attrs = resource.getAttributes();
    attrs.forEach(
        (attr, val) -> {
          logger.debug("Otel Resource Key: " + attr.getKey() + " Value: " + val);
        });

    SdkMeterProviderBuilder meterProviderBuilder =
        SdkMeterProvider.builder()
            .registerMetricReader(
                PeriodicMetricReader.builder(cloudMonitoringExporter)
                    .setInterval(java.time.Duration.ofSeconds(60))
                    .build())
            .addResource(resource);

    return OpenTelemetrySdk.builder().setMeterProvider(meterProviderBuilder.build()).build();
  }
}
