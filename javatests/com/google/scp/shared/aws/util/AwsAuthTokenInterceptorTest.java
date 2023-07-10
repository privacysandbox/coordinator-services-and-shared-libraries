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

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import java.io.IOException;
import java.net.URI;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import org.apache.hc.core5.http.HttpException;
import org.apache.hc.core5.http.HttpRequest;
import org.apache.hc.core5.http.Method;
import org.apache.hc.core5.http.io.support.ClassicRequestBuilder;
import org.apache.hc.core5.http.message.BasicClassicHttpRequest;
import org.apache.hc.core5.http.message.BasicHeader;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.MockedStatic;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import software.amazon.awssdk.auth.credentials.AwsBasicCredentials;
import software.amazon.awssdk.auth.credentials.AwsCredentials;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.AwsSessionCredentials;
import software.amazon.awssdk.regions.Region;

@RunWith(JUnit4.class)
public final class AwsAuthTokenInterceptorTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private AwsSessionCredentialsProvider sessionCredsProvider;
  @Mock private AwsCredentialsProvider longTermCredentialsProvider;
  HttpRequest signedRequest;
  private final AwsSessionCredentials sessionCredentials =
      AwsSessionCredentials.create("accessKeyId", "accessSecret", "sessionToken");
  private final AwsCredentials longTermCredentials =
      AwsBasicCredentials.create("accessKeyId", "accessSecret");
  private final String authEndpoint = "https://authEndpoint.com";

  private AwsAuthTokenInterceptor interceptor;

  @Before
  public void setup() {
    when(sessionCredsProvider.resolveCredentials()).thenReturn(sessionCredentials);
    when(longTermCredentialsProvider.resolveCredentials()).thenReturn(longTermCredentials);

    Date fixedDate = new GregorianCalendar(2020, Calendar.FEBRUARY, 11).getTime();
    signedRequest = ClassicRequestBuilder.create(Method.POST.name()).build();
    signedRequest.setHeader(new BasicHeader("Authorization", "dummySignature"));
    signedRequest.setHeader(new BasicHeader("X-Amz-Date", fixedDate.toString()));
  }

  @Test
  public void makeSignedHttpRequest_sessionCredentials_success() throws HttpException, IOException {
    BasicClassicHttpRequest request =
        new BasicClassicHttpRequest(Method.POST, URI.create("http://www.google.com"));
    String expectedAuthToken =
        "eyJzaWduYXR1cmUiOiJkdW1teVNpZ25hdHVyZSIsImFjY2Vzc19rZXkiOiJhY2Nlc3NLZXlJZCIsImFtel9kYXRlIj"
            + "oiVHVlIEZlYiAxMSAwMDowMDowMCBVVEMgMjAyMCJ9";
    interceptor =
        new AwsAuthTokenInterceptor(Region.US_WEST_1, authEndpoint, longTermCredentialsProvider);
    MockedStatic<AwsRequestSigner> requestSigner = Mockito.mockStatic(AwsRequestSigner.class);
    requestSigner
        .when(
            () ->
                AwsRequestSigner.makeSignedHttpRequest(
                    any(HttpRequest.class),
                    eq("execute-api"),
                    eq(Region.US_WEST_1),
                    eq(longTermCredentials)))
        .thenReturn(signedRequest);

    interceptor.process(request, null, null);

    String actualAuthToken = request.getHeader("x-auth-token").getValue();
    assertThat(actualAuthToken).isEqualTo(expectedAuthToken);

    requestSigner.close();
  }

  @Test
  public void makeSignedHttpRequest_longTermCredentials_success()
      throws HttpException, IOException {
    BasicClassicHttpRequest request =
        new BasicClassicHttpRequest(Method.POST, URI.create("http://www.google.com"));
    String expectedAuthToken =
        "eyJzaWduYXR1cmUiOiJkdW1teVNpZ25hdHVyZSIsInNlY3VyaXR5X3Rva2VuIjoic2Vzc2lvblRva2VuIiwiYWNjZXN"
            + "zX2tleSI6ImFjY2Vzc0tleUlkIiwiYW16X2RhdGUiOiJUdWUgRmViIDExIDAwOjAwOjAwIFVUQyAyMDIwIn0=";
    interceptor = new AwsAuthTokenInterceptor(Region.US_WEST_1, authEndpoint, sessionCredsProvider);
    MockedStatic<AwsRequestSigner> requestSigner = Mockito.mockStatic(AwsRequestSigner.class);
    requestSigner
        .when(
            () ->
                AwsRequestSigner.makeSignedHttpRequest(
                    any(HttpRequest.class),
                    eq("execute-api"),
                    eq(Region.US_WEST_1),
                    eq(sessionCredentials)))
        .thenReturn(signedRequest);

    interceptor.process(request, null, null);

    String actualAuthToken = request.getHeader("x-auth-token").getValue();
    assertThat(actualAuthToken).isEqualTo(expectedAuthToken);

    requestSigner.close();
  }
}
