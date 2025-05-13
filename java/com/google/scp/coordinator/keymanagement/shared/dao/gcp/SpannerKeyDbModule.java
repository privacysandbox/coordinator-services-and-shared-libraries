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

package com.google.scp.coordinator.keymanagement.shared.dao.gcp;

import com.google.cloud.NoCredentials;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.DatabaseId;
import com.google.cloud.spanner.InstanceId;
import com.google.cloud.spanner.LazySpannerInitializer;
import com.google.cloud.spanner.Spanner;
import com.google.cloud.spanner.SpannerOptions;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.coordinator.keymanagement.shared.dao.common.Annotations.KeyDbClient;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.shared.gcp.util.SpannerDatabaseConfig;

/** Module for spanner key db. */
public final class SpannerKeyDbModule extends AbstractModule {

  private static final LazySpannerInitializer SPANNER_INITIALIZER = new LazySpannerInitializer();

  /** Caller is expected to bind {@link SpannerDatabaseConfig}. */
  public SpannerKeyDbModule() {}

  @Provides
  @Singleton
  @KeyDbClient
  public DatabaseClient getDatabaseClient(SpannerDatabaseConfig config) throws Exception {
    if (config.endpointUrl().isPresent()) {
      return getDatabaseClientByEndpointUrl(config);
    }
    DatabaseId dbId =
        DatabaseId.of(config.gcpProjectId(), config.instanceId(), config.databaseName());
    return SPANNER_INITIALIZER.get().getDatabaseClient(dbId);
  }

  private static DatabaseClient getDatabaseClientByEndpointUrl(SpannerDatabaseConfig config) {
    String endpointUrl = config.endpointUrl().get();
    SpannerOptions.Builder spannerOptions =
        SpannerOptions.newBuilder().setProjectId(config.gcpProjectId());
    if (isEmulatorEndpoint(endpointUrl)) {
      spannerOptions.setEmulatorHost(endpointUrl).setCredentials(NoCredentials.getInstance());
    } else {
      spannerOptions.setHost(endpointUrl);
    }
    Spanner spanner = spannerOptions.build().getService();
    InstanceId instanceId = InstanceId.of(config.gcpProjectId(), config.instanceId());
    DatabaseId databaseId = DatabaseId.of(instanceId, config.databaseName());
    return spanner.getDatabaseClient(databaseId);
  }

  private static boolean isEmulatorEndpoint(String endpointUrl) {
    return !endpointUrl.toLowerCase().startsWith("https://");
  }

  @Override
  protected void configure() {
    bind(KeyDb.class).to(SpannerKeyDb.class);
  }
}
