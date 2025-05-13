package com.google.scp.shared.gcp.util;

import com.google.auto.value.AutoValue;
import java.util.Optional;

@AutoValue
public abstract class SpannerDatabaseConfig {

  public static Builder builder() {
    return new AutoValue_SpannerDatabaseConfig.Builder();
  }

  public abstract String instanceId();

  public abstract String databaseName();

  public abstract String gcpProjectId();

  /**
   * Overrides Spanner endpoint URL. Values that do not start with https:// are assumed to be
   * emulators for testing.
   */
  public abstract Optional<String> endpointUrl();

  @AutoValue.Builder
  public abstract static class Builder {

    public abstract Builder setInstanceId(String instanceId);

    public abstract Builder setDatabaseName(String dbName);

    public abstract Builder setGcpProjectId(String projectId);

    /**
     * Set the overriding Spanner endpoint URL. Values that do not start with https:// are assumed
     * to be emulators for testing.
     */
    public abstract Builder setEndpointUrl(Optional<String> endpointUrl);

    public abstract SpannerDatabaseConfig build();
  }
}
