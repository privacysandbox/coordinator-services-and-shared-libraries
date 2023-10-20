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
