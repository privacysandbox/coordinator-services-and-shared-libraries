package com.google.scp.coordinator.clients.configclient.gcp;

import com.google.auto.value.AutoValue;

/** Holds configuration parameters for {@code GcpCoordinatorClientConfigModule}. */
@AutoValue
public abstract class GcpCoordinatorClientConfig {

  /**
   * Returns an instance of the {@code GcpCoordinatorClientConfig.Builder} class. Default:
   * useLocalCredentials = true
   */
  public static Builder builder() {
    return new AutoValue_GcpCoordinatorClientConfig.Builder()
        .setPeerCoordinatorWipProvider("")
        .setPeerCoordinatorServiceAccount("")
        .setUseLocalCredentials(true);
  }

  /** Peer Coordinator Workload Identity Pool Provider. */
  public abstract String peerCoordinatorWipProvider();

  /**
   * Peer Coordinator Service Account which TEE can impersonate and has access to protected
   * resources.
   */
  public abstract String peerCoordinatorServiceAccount();

  /** Whether to use local, unattested credentials. */
  public abstract boolean useLocalCredentials();

  /** Builder class for the {@code GcpClientConfig} class. */
  @AutoValue.Builder
  public abstract static class Builder {

    /** Set the Peer Coordinator Workload Identity Pool Provider. */
    public abstract Builder setPeerCoordinatorWipProvider(String peerCoordinatorWipProvider);

    /** Set the Peer Coordinator Service Account which can be impersonated by TEE. */
    public abstract Builder setPeerCoordinatorServiceAccount(String peerCoordinatorServiceAccount);

    /** Set whether to use local, unattested credentials. */
    public abstract Builder setUseLocalCredentials(boolean useLocalCredentials);

    /**
     * Uses the builder to construct a new instance of the {@code GcpCoordinatorClientConfig} class.
     */
    public abstract GcpCoordinatorClientConfig build();
  }
}
