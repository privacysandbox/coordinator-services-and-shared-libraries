package com.google.scp.shared.gcp.util;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.auth.oauth2.IdTokenCredentials;
import com.google.auth.oauth2.IdTokenProvider;
import com.google.auth.oauth2.IdTokenProvider.Option;
import java.io.IOException;
import java.util.Arrays;

/** Provide gcp http interceptors */
public final class GcpHttpInterceptorUtil {

  /** Create http interceptor for gcp http clients with url as audience */
  public static org.apache.http.HttpRequestInterceptor createHttpInterceptor(String url) {
    return (request, context) -> {
      String identityToken = getIdTokenFromMetadataServer(url);
      request.addHeader("Authorization", String.format("Bearer %s", identityToken));
    };
  }

  /**
   * Create HTTP Interceptor for GCP HTTP clients with url as audience. This is for use when
   * communicating with the PBS server - not with GCP directly.
   *
   * @param url The TargetAudience url to use
   * @return A HttpRequestInterceptor that applies the generated GCP AccessToken to the intercepted
   *     HTTP request.
   */
  public static org.apache.hc.core5.http.HttpRequestInterceptor createPbsHttpInterceptor(
      String url) {
    return (request, entityDetails, context) -> {
      request.addHeader("x-auth-token", getIdTokenFromMetadataServer(url));
    };
  }

  private static IdTokenCredentials getIdTokenCredentials(String url) throws IOException {
    GoogleCredentials googleCredentials = GoogleCredentials.getApplicationDefault();
    IdTokenProvider idTokenProvider = (IdTokenProvider) googleCredentials;

    return IdTokenCredentials.newBuilder()
        .setIdTokenProvider(idTokenProvider)
        .setTargetAudience(url)
        // Setting the ID token options.
        .setOptions(Arrays.asList(Option.FORMAT_FULL, Option.LICENSES_TRUE))
        .build();
  }

  private static String getIdTokenFromMetadataServer(String url) throws IOException {
    IdTokenCredentials idTokenCredentials = getIdTokenCredentials(url);
    return idTokenCredentials.refreshAccessToken().getTokenValue();
  }
}
