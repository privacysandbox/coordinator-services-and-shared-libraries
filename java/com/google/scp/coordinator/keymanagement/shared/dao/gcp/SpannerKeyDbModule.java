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

/** Module for spanner key db. */
public final class SpannerKeyDbModule extends AbstractModule {

  private static final LazySpannerInitializer SPANNER_INITIALIZER = new LazySpannerInitializer();

  /** Caller is expected to bind {@link SpannerKeyDbConfig}. */
  public SpannerKeyDbModule() {}

  @Provides
  @Singleton
  @KeyDbClient
  public DatabaseClient getDatabaseClient(SpannerKeyDbConfig config) throws Exception {
    if (config.endpointUrl().isPresent()) {
      return getDatabaseClientByEndpointUrl(config);
    }
    DatabaseId dbId =
        DatabaseId.of(config.gcpProjectId(), config.spannerInstanceId(), config.spannerDbName());
    return SPANNER_INITIALIZER.get().getDatabaseClient(dbId);
  }

  private static DatabaseClient getDatabaseClientByEndpointUrl(SpannerKeyDbConfig config) {
    String endpointUrl = config.endpointUrl().get();
    SpannerOptions.Builder spannerOptions =
        SpannerOptions.newBuilder().setProjectId(config.gcpProjectId());
    if (isEmulatorEndpoint(endpointUrl)) {
      spannerOptions.setEmulatorHost(endpointUrl).setCredentials(NoCredentials.getInstance());
    } else {
      spannerOptions.setHost(endpointUrl);
    }
    Spanner spanner = spannerOptions.build().getService();
    InstanceId instanceId = InstanceId.of(config.gcpProjectId(), config.spannerInstanceId());
    DatabaseId databaseId = DatabaseId.of(instanceId, config.spannerDbName());
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
