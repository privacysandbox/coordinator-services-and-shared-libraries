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

package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import com.google.inject.AbstractModule;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.Annotations.CacheControlMaximum;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.Annotations.KeyLimit;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDbConfig;
import com.google.scp.coordinator.keymanagement.shared.dao.gcp.SpannerKeyDbModule;
import com.google.scp.shared.otel.OTelConfigurationModule;
import java.util.Map;
import java.util.Optional;

/**
 * Defines dependencies for GCP implementation of KeyService. It can be used for both Public Key and
 * Private Key Cloud Functions.
 */
public final class GcpKeyServiceModule extends AbstractModule {

  private static final String KEY_LIMIT_ENV_VAR = "KEY_LIMIT";
  private static final String READ_STALENESS_SEC_ENV_VAR = "READ_STALENESS_SEC";
  private static final String SPANNER_INSTANCE_ENV_VAR = "SPANNER_INSTANCE";
  private static final String SPANNER_DATABASE_ENV_VAR = "SPANNER_DATABASE";
  private static final String SPANNER_ENDPOINT = "SPANNER_ENDPOINT";
  private static final String PROJECT_ID_ENV_VAR = "PROJECT_ID";
  private static final String CACHE_CONTROL_MAXIMUM_ENV_VAR = "CACHE_CONTROL_MAXIMUM";

  /** Returns KeyLimit as Integer from environment variables. Default value of 5 */
  private Integer getKeyLimit() {
    Map<String, String> env = System.getenv();
    return Integer.valueOf(env.getOrDefault(KEY_LIMIT_ENV_VAR, "5"));
  }

  /** Returns ReadStalenessSeconds as Integer from environment variables. Default value of 15 */
  private Integer getReadStalenessSeconds() {
    Map<String, String> env = System.getenv();
    return Integer.valueOf(env.getOrDefault(READ_STALENESS_SEC_ENV_VAR, "15"));
  }

  /**
   * Returns CACHE_CONTROL_MAXIMUM as long from environment var. This value should reflect the
   * KeyRotationInterval time. Default value of 7 days in seconds.
   */
  private static Long getCacheControlMaximum() {
    Map<String, String> env = System.getenv();
    return Long.valueOf(env.getOrDefault(CACHE_CONTROL_MAXIMUM_ENV_VAR, "604800"));
  }

  @Override
  protected void configure() {
    Map<String, String> env = System.getenv();
    String spannerInstanceId = env.getOrDefault(SPANNER_INSTANCE_ENV_VAR, "keydbinstance");
    String spannerDatabaseId = env.getOrDefault(SPANNER_DATABASE_ENV_VAR, "keydb");
    String projectId = env.getOrDefault(PROJECT_ID_ENV_VAR, "adhcloud-tp1");
    String spannerEndpoint = env.get(SPANNER_ENDPOINT);

    // Service layer bindings
    bind(Long.class).annotatedWith(CacheControlMaximum.class).toInstance(getCacheControlMaximum());

    // Business layer bindings
    bind(Integer.class).annotatedWith(KeyLimit.class).toInstance(getKeyLimit());

    // Data layer bindings
    SpannerKeyDbConfig config =
        SpannerKeyDbConfig.builder()
            .setGcpProjectId(projectId)
            .setSpannerInstanceId(spannerInstanceId)
            .setSpannerDbName(spannerDatabaseId)
            .setReadStalenessSeconds(getReadStalenessSeconds())
            .setEndpointUrl(Optional.ofNullable(spannerEndpoint))
            .build();
    bind(SpannerKeyDbConfig.class).toInstance(config);
    install(new SpannerKeyDbModule());
    install(new OTelConfigurationModule());
  }
}
