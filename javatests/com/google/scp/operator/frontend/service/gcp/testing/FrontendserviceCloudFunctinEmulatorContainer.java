package com.google.scp.operator.frontend.service.gcp.testing;

import static com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer.startContainerAndConnectToSpannerAndPubSub;

import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.shared.testutils.gcp.CloudFunctionEmulatorContainer;
import com.google.scp.shared.testutils.gcp.PubSubEmulatorContainer;
import com.google.scp.shared.testutils.gcp.SpannerEmulatorContainer;

/** Cloud function emulator container for frontend service integration test */
public class FrontendserviceCloudFunctinEmulatorContainer extends AbstractModule {
  public FrontendserviceCloudFunctinEmulatorContainer() {}

  @Provides
  @Singleton
  public CloudFunctionEmulatorContainer getFunctionContainer(
      SpannerEmulatorContainer spannerEmulatorContainer,
      PubSubEmulatorContainer pubSubEmulatorContainer) {
    return startContainerAndConnectToSpannerAndPubSub(
        spannerEmulatorContainer,
        pubSubEmulatorContainer,
        "LocalFrontendServiceHttpCloudFunction_deploy.jar",
        "javatests/com/google/scp/operator/frontend/service/gcp/testing/",
        "com.google.scp.operator.frontend.service.gcp.testing.LocalFrontendServiceHttpFunction");
  }
}
