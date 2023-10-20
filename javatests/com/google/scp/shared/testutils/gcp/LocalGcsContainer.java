package com.google.scp.shared.testutils.gcp;

import org.testcontainers.containers.GenericContainer;
import org.testcontainers.containers.Network;
import org.testcontainers.containers.wait.strategy.Wait;
import org.testcontainers.utility.DockerImageName;

/** A testcontainer of GCS. */
public final class LocalGcsContainer extends GenericContainer<LocalGcsContainer> {

  private static final int HTTP_PORT = 9000;
  private static final DockerImageName LOCAL_GCS_IMAGE =
      DockerImageName.parse("gcr.io/cloud-devrel-public-resources/storage-testbench:v0.36.0");
  private String networkAlias;

  public LocalGcsContainer() {
    super(LOCAL_GCS_IMAGE);
    withExposedPorts(HTTP_PORT).waitingFor(Wait.forListeningPort()).start();
  }

  public LocalGcsContainer(Network network, String networkAlias) {
    super(LOCAL_GCS_IMAGE);
    this.networkAlias = networkAlias;
    withExposedPorts(HTTP_PORT)
        .waitingFor(Wait.forListeningPort())
        .withNetwork(network)
        .withNetworkAliases(networkAlias)
        .start();
  }

  @Override
  public String getContainerIpAddress() {
    return String.format("http://%s:%d", super.getContainerIpAddress(), getMappedPort(HTTP_PORT));
  }

  public String getContainerAliasAddress() {
    if (networkAlias != null && !networkAlias.isEmpty()) {
      return String.format("http://%s:%d", networkAlias, HTTP_PORT);
    } else {
      return getContainerIpAddress();
    }
  }
}
