package com.google.scp.shared.testutils.gcp;

import org.testcontainers.containers.GenericContainer;
import org.testcontainers.containers.Network;
import org.testcontainers.containers.wait.strategy.LogMessageWaitStrategy;
import org.testcontainers.utility.DockerImageName;

/**
 * Container for spanner emulator. Reference: https://github.com/testcontainers/testcontainers-java
 */
public final class SpannerEmulatorContainer extends GenericContainer<SpannerEmulatorContainer> {

  /** The value of the spanner emulator's GRPC port. */
  public static final int GRPC_PORT = 9010;
  /** The value of the spanner emulator's HTTP port. */
  public static final int HTTP_PORT = 9050;

  private static final DockerImageName DEFAULT_IMAGE_NAME =
      DockerImageName.parse("gcr.io/cloud-spanner-emulator/emulator");
  private String networkAlias;

  /** Constructor for the {@code SpannerEmulatorContainer} class. */
  public SpannerEmulatorContainer(final DockerImageName dockerImageName) {
    super(dockerImageName);

    dockerImageName.assertCompatibleWith(DEFAULT_IMAGE_NAME);

    withExposedPorts(HTTP_PORT, GRPC_PORT);
    setWaitStrategy(
        new LogMessageWaitStrategy().withRegEx(".*Cloud Spanner emulator running\\..*"));
  }

  public SpannerEmulatorContainer(
      final DockerImageName dockerImageName, Network network, String networkAlias) {
    super(dockerImageName);

    this.networkAlias = networkAlias;
    withCommand(
        "/bin/sh",
        "-c",
        "gcloud beta emulators spanner start --rest-port 9050 --host-port 0.0.0.0:9010");
    setWaitStrategy(
        new LogMessageWaitStrategy().withRegEx(".*Cloud Spanner emulator running\\..*"));
    withExposedPorts(GRPC_PORT, HTTP_PORT)
        .withNetwork(network)
        .withNetworkAliases(networkAlias)
        .start();
  }

  /**
   * @return a <code>host:port</code> pair corresponding to the address on which the emulator's gRPC
   *     endpoint is reachable from the test host machine. Directly usable as a parameter to the
   *     com.google.cloud.spanner.SpannerOptions.Builder#setEmulatorHost(java.lang.String) method.
   */
  public String getEmulatorGrpcEndpoint() {
    return getContainerIpAddress() + ":" + getMappedPort(GRPC_PORT);
  }

  /**
   * @return a <code>host:port</code> pair corresponding to the address on which the emulator's HTTP
   *     REST endpoint is reachable from the test host machine.
   */
  public String getEmulatorHttpEndpoint() {
    return getContainerIpAddress() + ":" + getMappedPort(HTTP_PORT);
  }

  public String getContainerHttpAddress() {
    // return String.format("http://%s:%d", networkAlias, HTTP_PORT);
    return getContainerInfo().getNetworkSettings().getNetworks().entrySet().stream()
            .findFirst()
            .get()
            .getValue()
            .getIpAddress()
        + ":"
        + HTTP_PORT;
  }

  public String getContainerGrpcAddress() {
    return getContainerInfo().getNetworkSettings().getNetworks().entrySet().stream()
            .findFirst()
            .get()
            .getValue()
            .getIpAddress()
        + ":"
        + GRPC_PORT;
  }
}
