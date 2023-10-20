package com.google.scp.coordinator.keymanagement.shared.dao.gcp;

import com.google.auto.value.AutoValue;
import java.util.Optional;

/** Holds configuration parameters for {@code SpannerKeyDbModule} */
// Consider consolidating with SpannerMetadataDbConfig
@AutoValue
public abstract class SpannerKeyDbConfig {

  public static SpannerKeyDbConfig.Builder builder() {
    return new AutoValue_SpannerKeyDbConfig.Builder();
  }

  /** Instance ID of the job key DB */
  public abstract String spannerInstanceId();

  /** Key DB name */
  public abstract String spannerDbName();

  /** GCP project ID */
  public abstract String gcpProjectId();

  /** Acceptable read staleness in seconds. */
  public abstract int readStalenessSeconds();

  /**
   * Overrides Spanner endpoint URL. Values that do not start with https:// are assumed to be
   * emulators for testing.
   */
  public abstract Optional<String> endpointUrl();

  @AutoValue.Builder
  public abstract static class Builder {

    public abstract Builder setSpannerInstanceId(String instanceId);

    public abstract Builder setSpannerDbName(String dbName);

    public abstract Builder setGcpProjectId(String projectId);

    public abstract Builder setReadStalenessSeconds(int readStalenessSeconds);

    /**
     * Set the overriding Spanner endpoint URL. Values that do not start with https:// are assumed
     * to be emulators for testing.
     */
    public abstract Builder setEndpointUrl(Optional<String> endpointUrl);

    public abstract SpannerKeyDbConfig build();
  }
}
