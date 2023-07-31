/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.shared.aws.util;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.inject.Inject;
import java.io.IOException;
import java.net.URISyntaxException;
import java.util.Base64;
import java.util.HashMap;
import java.util.Map;
import org.apache.hc.core5.http.ClassicHttpRequest;
import org.apache.hc.core5.http.EntityDetails;
import org.apache.hc.core5.http.HttpException;
import org.apache.hc.core5.http.HttpRequest;
import org.apache.hc.core5.http.HttpRequestInterceptor;
import org.apache.hc.core5.http.Method;
import org.apache.hc.core5.http.ProtocolException;
import org.apache.hc.core5.http.io.support.ClassicRequestBuilder;
import org.apache.hc.core5.http.protocol.HttpContext;
import software.amazon.awssdk.auth.credentials.AwsCredentials;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.AwsSessionCredentials;
import software.amazon.awssdk.regions.Region;

/**
 * Request Interceptor that generates an auth token from AWS Sigv4 signature of the request and adds
 * it to the request header
 */
public class AwsAuthTokenInterceptor implements HttpRequestInterceptor {

  private final Region awsRegion;
  private final AwsCredentialsProvider credsProvider;
  private final String authEndpoint;

  @Inject
  public AwsAuthTokenInterceptor(
      Region awsRegion, String authEndpoint, AwsCredentialsProvider credsProvider) {
    this.awsRegion = awsRegion;
    this.authEndpoint = authEndpoint;
    this.credsProvider = credsProvider;
  }

  @Override
  public void process(HttpRequest httpRequest, EntityDetails entityDetails, HttpContext httpContext)
      throws HttpException, IOException {
    AwsCredentials credentials = credsProvider.resolveCredentials();
    /*
     * Clone the original request but set the request URI to the auth endpoint as we will sign the
     * request to be used against the auth endpoint. Always use POST since auth endpoint only accepts POST requests
     */
    ClassicHttpRequest clonedRequest =
        ClassicRequestBuilder.create(Method.POST.name()).setUri(authEndpoint).build();
    try {
      HttpRequest sdkRequest =
          AwsRequestSigner.makeSignedHttpRequest(
              clonedRequest, "execute-api", this.awsRegion, credentials);
      httpRequest.addHeader("x-auth-token", getAuthToken(sdkRequest, credentials));
    } catch (URISyntaxException exception) {
      throw new HttpException("Syntax error in URI.", exception);
    }
  }

  private static String getAuthToken(HttpRequest sdkRequest, AwsCredentials credentials)
      throws JsonProcessingException, ProtocolException {
    Map<String, String> json_token_map = new HashMap<>();
    json_token_map.put("access_key", credentials.accessKeyId());
    json_token_map.put("signature", sdkRequest.getHeader("Authorization").getValue());
    json_token_map.put("amz_date", sdkRequest.getHeader("X-Amz-Date").getValue());
    if (credentials instanceof AwsSessionCredentials) {
      json_token_map.put("security_token", ((AwsSessionCredentials) credentials).sessionToken());
    }
    String json_token_str = new ObjectMapper().writeValueAsString(json_token_map);
    return Base64.getEncoder().encodeToString(json_token_str.getBytes());
  }
}
