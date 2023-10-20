package com.google.scp.shared.testutils.gcp;

import com.google.acai.BeforeSuite;
import com.google.acai.TestingService;
import com.google.inject.Inject;

/** Acai-managed service that starts local PubSub instance */
public final class PubSubLocalService implements TestingService {

  @Inject private PubSubEmulatorContainer localPubSub;

  @BeforeSuite
  void startPubSub() {
    localPubSub.start();
  }
}
