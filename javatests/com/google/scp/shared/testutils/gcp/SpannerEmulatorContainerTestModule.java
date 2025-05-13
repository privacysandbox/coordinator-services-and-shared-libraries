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

package com.google.scp.shared.testutils.gcp;

import com.google.cloud.NoCredentials;
import com.google.cloud.spanner.Database;
import com.google.cloud.spanner.DatabaseAdminClient;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.InstanceAdminClient;
import com.google.cloud.spanner.InstanceConfigId;
import com.google.cloud.spanner.InstanceId;
import com.google.cloud.spanner.InstanceInfo;
import com.google.cloud.spanner.Spanner;
import com.google.cloud.spanner.SpannerOptions;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.shared.gcp.util.SpannerDatabaseConfig;
import java.time.Clock;
import java.util.Collections;
import java.util.concurrent.ExecutionException;
import org.testcontainers.utility.DockerImageName;

public class SpannerEmulatorContainerTestModule extends AbstractModule {

  private final SpannerDatabaseConfig config;
  private final Iterable<String> createTableStatements;

  public SpannerEmulatorContainerTestModule(SpannerDatabaseConfig config) {
    this.config = config;
    this.createTableStatements = Collections.emptyList();
  }

  public SpannerEmulatorContainerTestModule(
      SpannerDatabaseConfig config, Iterable<String> createTableStatements) {
    this.config = config;
    this.createTableStatements = createTableStatements;
  }

  /** Container that runs local Spanner instance */
  @Provides
  @Singleton
  public SpannerEmulatorContainer getSpannerContainer() {
    return new SpannerEmulatorContainer(
        DockerImageName.parse("gcr.io/cloud-spanner-emulator/emulator:1.5.29"));
  }

  @Provides
  @Singleton
  public DatabaseClient getDatabaseClient(SpannerEmulatorContainer emulator)
      throws ExecutionException, InterruptedException {
    Spanner spanner =
        SpannerOptions.newBuilder()
            .setEmulatorHost(emulator.getEmulatorGrpcEndpoint())
            .setCredentials(NoCredentials.getInstance())
            .setProjectId(config.gcpProjectId())
            .build()
            .getService();

    createInstance(spanner);
    Database database = createDatabase(spanner);
    return spanner.getDatabaseClient(database.getId());
  }

  /** Provides an instance of the {@code Clock} class. */
  @Provides
  @Singleton
  public Clock provideClock() {
    return Clock.systemUTC();
  }

  private void createInstance(Spanner spanner) throws InterruptedException, ExecutionException {
    InstanceConfigId instanceConfig = InstanceConfigId.of(config.gcpProjectId(), "emulator-config");
    InstanceId instanceId = InstanceId.of(config.gcpProjectId(), config.instanceId());
    InstanceAdminClient insAdminClient = spanner.getInstanceAdminClient();
    insAdminClient
        .createInstance(
            InstanceInfo.newBuilder(instanceId)
                .setNodeCount(1)
                .setDisplayName("Test instance")
                .setInstanceConfigId(instanceConfig)
                .build())
        .get();
  }

  private Database createDatabase(Spanner spanner) throws InterruptedException, ExecutionException {
    DatabaseAdminClient dbAdminClient = spanner.getDatabaseAdminClient();
    return dbAdminClient
        .createDatabase(config.instanceId(), config.databaseName(), createTableStatements)
        .get();
  }
}
