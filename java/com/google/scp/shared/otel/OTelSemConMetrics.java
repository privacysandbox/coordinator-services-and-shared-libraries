package com.google.scp.shared.otel;

import java.util.Arrays;
import java.util.List;

public class OTelSemConMetrics {
  private OTelSemConMetrics() {}

  public static final String HTTP_SERVER_REQUEST_DURATION = "http.server.request.duration";

  public static final List<Double> LATENCY_BUCKETS =
      Arrays.asList(
          0.005, 0.01, 0.025, 0.05, 0.075, 0.1, 0.25, 0.5, 0.75, 1.0, 2.0, 5.0, 7.5, 10.0);
}
