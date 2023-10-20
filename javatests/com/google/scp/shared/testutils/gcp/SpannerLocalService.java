package com.google.scp.shared.testutils.gcp;

import com.google.acai.BeforeSuite;
import com.google.acai.TestingService;
import com.google.inject.Inject;

/** Acai-managed service that starts local Spanner instance */
public final class SpannerLocalService implements TestingService {

  @Inject private SpannerEmulatorContainer localSpanner;

  @BeforeSuite
  void startSpanner() {
    localSpanner.start();
  }
}
