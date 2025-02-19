/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.shared.gcp.util;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.auth.oauth2.IdTokenCredentials;
import com.google.auth.oauth2.IdTokenProvider;
import com.google.auth.oauth2.IdTokenProvider.Option;
import java.io.IOException;
import java.time.Duration;
import java.util.Arrays;
import java.util.function.Consumer;
import org.apache.hc.core5.http.EntityDetails;
import org.apache.hc.core5.http.HttpRequest;
import org.apache.hc.core5.http.protocol.HttpContext;

/** Provide gcp http interceptors */
public final class GcpHttpInterceptorUtil {

  /** Create http interceptor for gcp http clients with url as audience */
  public static org.apache.http.HttpRequestInterceptor createHttpInterceptor(String url)
      throws IOException {
    return createHttpInterceptor(GoogleCredentials.getApplicationDefault(), url);
  }

  public static org.apache.http.HttpRequestInterceptor createHttpInterceptor(
      GoogleCredentials credentials, String url) {
    return new GcpHttpInterceptor(credentials, url);
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
      GoogleCredentials credentials, String url) {
    return new GcpPbsHttpInterceptor(credentials, url);
  }

  public static org.apache.hc.core5.http.HttpRequestInterceptor createPbsHttpInterceptor(String url)
      throws IOException {
    return new GcpPbsHttpInterceptor(GoogleCredentials.getApplicationDefault(), url);
  }

  private abstract static class GcpHttpInterceptorBase {

    final IdTokenCredentials idTokenCredentials;

    public GcpHttpInterceptorBase(
        GoogleCredentials sourceCredentials,
        String url,
        Consumer<IdTokenCredentials.Builder>... extraSettings) {

      IdTokenCredentials.Builder builder =
          IdTokenCredentials.newBuilder()
              .setIdTokenProvider((IdTokenProvider) sourceCredentials)
              .setTargetAudience(url)
              // Setting the ID token options.
              .setOptions(
                  Arrays.asList(Option.FORMAT_FULL, Option.LICENSES_TRUE, Option.INCLUDE_EMAIL));
      for (Consumer<IdTokenCredentials.Builder> extraSetting : extraSettings) {
        extraSetting.accept(builder);
      }
      idTokenCredentials = builder.build();
    }

    public final String getIdToken() throws IOException {
      idTokenCredentials.refreshIfExpired();
      return idTokenCredentials.getIdToken().getTokenValue();
    }
  }

  private static final class GcpHttpInterceptor extends GcpHttpInterceptorBase
      implements org.apache.http.HttpRequestInterceptor {

    public GcpHttpInterceptor(GoogleCredentials sourceCredentials, String url) {
      super(sourceCredentials, url);
    }

    @Override
    public void process(
        org.apache.http.HttpRequest httpRequest, org.apache.http.protocol.HttpContext httpContext)
        throws IOException {
      httpRequest.addHeader("Authorization", String.format("Bearer %s", getIdToken()));
    }
  }

  private static final class GcpPbsHttpInterceptor extends GcpHttpInterceptorBase
      implements org.apache.hc.core5.http.HttpRequestInterceptor {

    public GcpPbsHttpInterceptor(GoogleCredentials sourceCredentials, String url) {
      super(
          sourceCredentials,
          url,
          builder ->
              builder
                  .setExpirationMargin(Duration.ofMinutes(10))
                  .setRefreshMargin(Duration.ofMinutes(1)));
    }

    @Override
    public void process(
        HttpRequest httpRequest, EntityDetails entityDetails, HttpContext httpContext)
        throws IOException {
      httpRequest.addHeader("x-auth-token", getIdToken());
    }
  }
}
