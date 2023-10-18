package com.google.scp.shared.testutils.gcp;

import org.testcontainers.containers.GenericContainer;
import org.testcontainers.containers.wait.strategy.LogMessageWaitStrategy;
import org.testcontainers.utility.DockerImageName;

/**
 * Container for pub sub emulator. Reference: https://github.com/testcontainers/testcontainers-java
 */
public final class PubSubEmulatorContainer extends GenericContainer<PubSubEmulatorContainer> {

  private static final DockerImageName DEFAULT_IMAGE_NAME =
      DockerImageName.parse("gcr.io/google.com/cloudsdktool/cloud-sdk");

  private static final String CMD = "gcloud beta emulators pubsub start --host-port 0.0.0.0:8085";
  private static final int PORT = 8085;

  public PubSubEmulatorContainer(final DockerImageName dockerImageName) {
    this(dockerImageName, false);
  }

  public PubSubEmulatorContainer(final DockerImageName dockerImageName, boolean startImmediately) {
    super(dockerImageName);

    dockerImageName.assertCompatibleWith(DEFAULT_IMAGE_NAME);

    withExposedPorts(8085);
    setWaitStrategy(new LogMessageWaitStrategy().withRegEx("(?s).*started.*$"));
    withCommand("/bin/sh", "-c", CMD);
    if (startImmediately) {
      start();
    }
  }

  /**
   * @return a <code>host:port</code> pair corresponding to the address on which the emulator is
   *     reachable from the test host machine. Directly usable as a parameter to the
   *     io.grpc.ManagedChannelBuilder#forTarget(java.lang.String) method.
   */
  public String getEmulatorEndpoint() {
    return getContainerIpAddress() + ":" + getMappedPort(PORT);
  }
}
