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

package com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener;

import com.google.auto.value.AutoValue;
import java.util.Optional;

/** Configuration parameters used by the PubSubListeners. */
@AutoValue
public abstract class PubSubListenerConfig {

  /** GCP pubsub endpoint URL to override the default value. Empty value is ignored. */
  public abstract Optional<String> endpointUrl();

  /** The timeout in seconds when waiting for the subscriber to terminate. Used in tests only. */
  public abstract Optional<Integer> timeoutInSeconds();

  public static PubSubListenerConfig.Builder newBuilder() {
    return new AutoValue_PubSubListenerConfig.Builder();
  }

  @AutoValue.Builder
  public abstract static class Builder {

    /** Sets the GCP pubsub endpoint URL to override the default value. Empty value is ignored. */
    public abstract Builder setEndpointUrl(Optional<String> endpointUrl);

    /**
     * Sets the timeout in seconds when waiting for the subscriber to terminate. Used in tests only.
     */
    public abstract Builder setTimeoutInSeconds(Optional<Integer> timeout);

    public abstract PubSubListenerConfig build();
  }
}
