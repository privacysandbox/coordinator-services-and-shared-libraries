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
