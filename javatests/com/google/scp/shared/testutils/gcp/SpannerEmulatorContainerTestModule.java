package com.google.scp.shared.testutils.gcp;

import com.google.cloud.NoCredentials;
import com.google.cloud.spanner.DatabaseAdminClient;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.DatabaseId;
import com.google.cloud.spanner.InstanceAdminClient;
import com.google.cloud.spanner.InstanceConfigId;
import com.google.cloud.spanner.InstanceId;
import com.google.cloud.spanner.InstanceInfo;
import com.google.cloud.spanner.Spanner;
import com.google.cloud.spanner.SpannerOptions;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import java.time.Clock;
import java.util.concurrent.ExecutionException;
import org.testcontainers.utility.DockerImageName;

public class SpannerEmulatorContainerTestModule extends AbstractModule {

  private final String projectName;
  private final String instanceName;
  private final String databaseName;
  private final Iterable<String> createTableStatements;

  public SpannerEmulatorContainerTestModule(
      String projectName,
      String instanceName,
      String databaseName,
      Iterable<String> createTableStatements) {
    this.projectName = projectName;
    this.instanceName = instanceName;
    this.databaseName = databaseName;
    this.createTableStatements = createTableStatements;
  }

  /** Container that runs local Spanner instance */
  @Provides
  @Singleton
  public SpannerEmulatorContainer getSpannerContainer() {
    return new SpannerEmulatorContainer(
        DockerImageName.parse("gcr.io/cloud-spanner-emulator/emulator:1.4.0"));
  }

  @Provides
  @Singleton
  public DatabaseClient getDatabaseClient(SpannerEmulatorContainer emulator)
      throws ExecutionException, InterruptedException {
    SpannerOptions options =
        SpannerOptions.newBuilder()
            .setEmulatorHost(emulator.getEmulatorGrpcEndpoint())
            .setCredentials(NoCredentials.getInstance())
            .setProjectId(projectName)
            .build();
    Spanner spanner = options.getService();
    InstanceId instanceId = createInstance(spanner);
    createDatabase(spanner);

    DatabaseId databaseId = DatabaseId.of(instanceId, databaseName);
    return spanner.getDatabaseClient(databaseId);
  }

  /** Provides an instance of the {@code Clock} class. */
  @Provides
  @Singleton
  public Clock provideClock() {
    return Clock.systemUTC();
  }

  private void createDatabase(Spanner spanner) throws InterruptedException, ExecutionException {
    DatabaseAdminClient dbAdminClient = spanner.getDatabaseAdminClient();
    dbAdminClient.createDatabase(instanceName, databaseName, createTableStatements).get();
  }

  private InstanceId createInstance(Spanner spanner)
      throws InterruptedException, ExecutionException {
    InstanceConfigId instanceConfig = InstanceConfigId.of(projectName, "emulator-config");
    InstanceId instanceId = InstanceId.of(projectName, instanceName);
    InstanceAdminClient insAdminClient = spanner.getInstanceAdminClient();
    insAdminClient
        .createInstance(
            InstanceInfo.newBuilder(instanceId)
                .setNodeCount(1)
                .setDisplayName("Test instance")
                .setInstanceConfigId(instanceConfig)
                .build())
        .get();
    return instanceId;
  }
}
