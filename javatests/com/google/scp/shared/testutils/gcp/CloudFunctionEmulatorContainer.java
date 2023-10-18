package com.google.scp.shared.testutils.gcp;

import java.util.Map;
import java.util.Optional;
import org.testcontainers.containers.GenericContainer;
import org.testcontainers.utility.DockerImageName;
import org.testcontainers.utility.MountableFile;

/** Class for configuring a container with a cloud function emulator. */
public final class CloudFunctionEmulatorContainer
    extends GenericContainer<CloudFunctionEmulatorContainer> {

  private static final String invokerJarFilename = "java-function-invoker-1.1.0.jar";
  private static final String invokerJarPath =
      "external/maven/v1/https/repo1.maven.org/maven2/com/google/cloud/functions/invoker/java-function-invoker/1.1.0/"
          + invokerJarFilename;
  private static final int invokerPort = 8080; // default internal port for the invoker jar process

  private final String functionFilename;
  private final String functionJarPath;
  private final String functionClassTarget;

  public CloudFunctionEmulatorContainer(
      DockerImageName dockerImageName,
      String functionFilename,
      String functionJarPath,
      String functionClassTarget) {
    super(dockerImageName);
    this.functionFilename = functionFilename;
    this.functionJarPath = functionJarPath;
    this.functionClassTarget = functionClassTarget;
    withExposedPorts(invokerPort);
    withCopyFileToContainer(MountableFile.forHostPath(invokerJarPath), "/");
    withCopyFileToContainer(MountableFile.forHostPath(getFunctionJarPath()), "/");
    withCommand("/bin/sh", "-c", getContainerStartupCommand());
  }

  /**
   * Start a container that runs a local cloud function emulator and connects to the spanner
   * emulator.
   */
  public static CloudFunctionEmulatorContainer startContainerAndConnectToSpanner(
      SpannerEmulatorContainer spannerEmulatorContainer,
      String functionFilename,
      String functionJarPath,
      String functionClassTarget) {
    return startContainerAndConnectToSpannerWithEnvs(
        spannerEmulatorContainer,
        Optional.empty(),
        functionFilename,
        functionJarPath,
        functionClassTarget);
  }

  public static CloudFunctionEmulatorContainer startContainerAndConnectToSpannerWithEnvs(
      SpannerEmulatorContainer spannerEmulatorContainer,
      Optional<Map<String, String>> envVariables,
      String functionFilename,
      String functionJarPath,
      String functionClassTarget) {
    CloudFunctionEmulatorContainer container =
        new CloudFunctionEmulatorContainer(
                DockerImageName.parse("openjdk:jdk-slim"),
                functionFilename,
                functionJarPath,
                functionClassTarget)
            .dependsOn(spannerEmulatorContainer)
            .withEnv(
                "SPANNER_EMULATOR_HOST",
                "http://"
                    + spannerEmulatorContainer
                        .getContainerInfo()
                        .getNetworkSettings()
                        .getNetworks()
                        .entrySet()
                        .stream()
                        .findFirst()
                        .get()
                        .getValue()
                        .getIpAddress()
                    + ":"
                    + SpannerEmulatorContainer.GRPC_PORT);
    if (envVariables.isPresent()) {
      container.withEnv(envVariables.get());
    }
    container.start();
    return container;
  }

  public static CloudFunctionEmulatorContainer startContainerAndConnectToSpannerAndPubSub(
      SpannerEmulatorContainer spannerEmulatorContainer,
      PubSubEmulatorContainer pubSubEmulatorContainer,
      String functionFilename,
      String functionJarPath,
      String functionClassTarget) {
    var container =
        new CloudFunctionEmulatorContainer(
                DockerImageName.parse("openjdk:jdk-slim"),
                functionFilename,
                functionJarPath,
                functionClassTarget)
            .dependsOn(spannerEmulatorContainer)
            .withEnv(
                "SPANNER_EMULATOR_HOST",
                "http://"
                    + spannerEmulatorContainer
                        .getContainerInfo()
                        .getNetworkSettings()
                        .getNetworks()
                        .entrySet()
                        .stream()
                        .findFirst()
                        .get()
                        .getValue()
                        .getIpAddress()
                    + ":"
                    + SpannerEmulatorContainer.GRPC_PORT)
            .dependsOn(pubSubEmulatorContainer)
            .withEnv(
                "PUBSUB_EMULATOR_HOST",
                pubSubEmulatorContainer
                        .getContainerInfo()
                        .getNetworkSettings()
                        .getNetworks()
                        .entrySet()
                        .stream()
                        .findFirst()
                        .get()
                        .getValue()
                        .getIpAddress()
                    + ":"
                    + pubSubEmulatorContainer.getExposedPorts().get(0));
    container.start();
    return container;
  }

  /**
   * @return a <code>host:port</code> pair corresponding to the address on which the emulator is
   *     reachable from the test host machine.
   */
  public String getEmulatorEndpoint() {
    return getHost() + ":" + getMappedPort(invokerPort);
  }

  /** Filename for the cloud function jar (must contain all dependencies). */
  private String getFunctionFilename() {
    return functionFilename;
  }

  /**
   * Path to the location of the cloud function jar at runtime. Include the filename using the
   * method.
   */
  private String getFunctionJarPath() {
    return functionJarPath + getFunctionFilename();
  }

  /** Fully qualified name of the Java class acting as the cloud function. */
  private String getFunctionClassTarget() {
    return functionClassTarget;
  }

  private String getContainerStartupCommand() {
    return "java -jar "
        + "./"
        + invokerJarFilename
        + " --classpath "
        + "./"
        + getFunctionFilename()
        + " --target "
        + getFunctionClassTarget();
  }
}
