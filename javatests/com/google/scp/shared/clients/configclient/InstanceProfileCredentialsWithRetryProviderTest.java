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

package com.google.scp.shared.clients.configclient;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockserver.model.HttpRequest.request;
import static org.mockserver.model.HttpResponse.response;

import com.google.scp.shared.clients.configclient.aws.InstanceProfileCredentialsWithRetryProvider;
import io.github.resilience4j.core.IntervalFunction;
import io.github.resilience4j.retry.Retry;
import io.github.resilience4j.retry.RetryConfig;
import io.github.resilience4j.retry.RetryRegistry;
import java.time.Duration;
import java.time.Instant;
import java.util.concurrent.TimeUnit;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockserver.client.MockServerClient;
import org.mockserver.junit.MockServerRule;
import org.mockserver.matchers.Times;
import org.mockserver.model.Delay;
import org.mockserver.model.Header;
import org.mockserver.model.NottableString;
import org.mockserver.verify.VerificationTimes;
import software.amazon.awssdk.auth.credentials.AwsCredentials;
import software.amazon.awssdk.core.SdkSystemSetting;
import software.amazon.awssdk.core.exception.SdkClientException;
import software.amazon.awssdk.core.util.SdkUserAgent;
import software.amazon.awssdk.utils.DateUtils;

@RunWith(JUnit4.class)
public class InstanceProfileCredentialsWithRetryProviderTest {
  private static final String stubToken = "some-token";
  private static final String stubProfile = "some-profile";
  private static final String TOKEN_RESOURCE_PATH = "/latest/api/token";
  private static final String CREDENTIALS_RESOURCE_PATH =
      "/latest/meta-data/iam/security-credentials/";
  private static final String STUB_CREDENTIALS =
      "{\"AccessKeyId\":\"ACCESS_KEY_ID\",\"SecretAccessKey\":\"SECRET_ACCESS_KEY\",\"Expiration\":\""
          + DateUtils.formatIso8601Date(Instant.now().plus(Duration.ofDays(1)))
          + "\"}";
  private static final String TOKEN_HEADER = "x-aws-ec2-metadata-token";
  private static final String EC2_METADATA_TOKEN_TTL_HEADER =
      "x-aws-ec2-metadata-token-ttl-seconds";
  private static final RetryRegistry registry =
      RetryRegistry.of(
          RetryConfig.<AwsCredentials>custom()
              .maxAttempts(4)
              .intervalFunction(IntervalFunction.ofExponentialBackoff(Duration.ofMillis(10), 2))
              .retryExceptions(RuntimeException.class)
              .build());
  private static final Retry retryConfig = registry.retry("credentialsRetryConfig");

  @Rule public MockServerRule mockServerRule = new MockServerRule(this);

  // Provided from the mock server rule
  private MockServerClient mockServerClient;

  private AwsCredentials resolveCredentials() {
    InstanceProfileCredentialsWithRetryProvider provider =
        InstanceProfileCredentialsWithRetryProvider.builder()
            .endpoint("http://localhost:" + mockServerClient.getPort())
            .retryConfig(retryConfig)
            .build();
    return provider.resolveCredentials();
  }

  @Test
  public void resolveCredentials_requestsIncludeUserAgent() {
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withBody(stubToken));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH))
        .respond(response().withBody(stubProfile));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH + stubProfile))
        .respond(response().withBody(STUB_CREDENTIALS));

    resolveCredentials();

    String userAgentHeader = "User-Agent";
    String expectedUserAgent = SdkUserAgent.create().userAgent();
    mockServerClient.verify(
        request()
            .withMethod("PUT")
            .withPath(TOKEN_RESOURCE_PATH)
            .withHeader(userAgentHeader, expectedUserAgent),
        VerificationTimes.exactly(1));
    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH)
            .withHeader(userAgentHeader, expectedUserAgent),
        VerificationTimes.exactly(1));
    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH + stubProfile)
            .withHeader(userAgentHeader, expectedUserAgent),
        VerificationTimes.exactly(1));
  }

  @Test
  public void resolveCredentials_metadataLookupDisabled_throws() {
    try {
      System.setProperty(SdkSystemSetting.AWS_EC2_METADATA_DISABLED.property(), "true");

      Exception ex = assertThrows(SdkClientException.class, this::resolveCredentials);

      assertThat(ex.getMessage()).startsWith("IMDS credentials have been disabled");
    } finally {
      System.clearProperty(SdkSystemSetting.AWS_EC2_METADATA_DISABLED.property());
    }
  }

  @Test
  public void resolveCredentials_endpointSettingEmpty_throws() {
    var stored = System.getProperty(SdkSystemSetting.AWS_EC2_METADATA_SERVICE_ENDPOINT.property());
    try {
      System.setProperty(SdkSystemSetting.AWS_EC2_METADATA_SERVICE_ENDPOINT.property(), "");

      Exception ex = assertThrows(SdkClientException.class, this::resolveCredentials);

      assertThat(ex.getMessage()).startsWith("Failed to load credentials from IMDS");
    } finally {
      if (stored == null) {
        System.clearProperty(SdkSystemSetting.AWS_EC2_METADATA_SERVICE_ENDPOINT.property());
      } else {
        System.setProperty(SdkSystemSetting.AWS_EC2_METADATA_SERVICE_ENDPOINT.property(), stored);
      }
    }
  }

  @Test
  public void resolveCredentials_endpointSettingHostNotExists_throws() {
    var stored = System.getProperty(SdkSystemSetting.AWS_EC2_METADATA_SERVICE_ENDPOINT.property());
    try {
      System.setProperty(
          SdkSystemSetting.AWS_EC2_METADATA_SERVICE_ENDPOINT.property(),
          "some-host-that-does-not-exist");

      Exception ex = assertThrows(SdkClientException.class, this::resolveCredentials);

      assertThat(ex.getMessage()).startsWith("Failed to load credentials from IMDS");
    } finally {
      if (stored == null) {
        System.clearProperty(SdkSystemSetting.AWS_EC2_METADATA_SERVICE_ENDPOINT.property());
      } else {
        System.setProperty(SdkSystemSetting.AWS_EC2_METADATA_SERVICE_ENDPOINT.property(), stored);
      }
    }
  }

  @Test
  public void resolveCredentials_queriesTokenResource() {
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withBody(stubToken));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH))
        .respond(response().withBody(stubProfile));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH + stubProfile))
        .respond(response().withBody(STUB_CREDENTIALS));

    resolveCredentials();

    mockServerClient.verify(
        request()
            .withMethod("PUT")
            .withPath(TOKEN_RESOURCE_PATH)
            .withHeader(EC2_METADATA_TOKEN_TTL_HEADER, "21600"),
        VerificationTimes.exactly(1));
  }

  @Test
  public void resolveCredentials_queriesTokenResource_includedInCredentialsRequests() {
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withBody(stubToken));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH))
        .respond(response().withBody(stubProfile));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH + stubProfile))
        .respond(response().withBody(STUB_CREDENTIALS));

    resolveCredentials();

    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH)
            .withHeader(TOKEN_HEADER, stubToken),
        VerificationTimes.exactly(1));
    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH + stubProfile)
            .withHeader(TOKEN_HEADER, stubToken),
        VerificationTimes.exactly(1));
  }

  @Test
  public void resolveCredentials_retryThenSucceed() {
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withBody(stubToken));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH))
        .respond(response().withBody(stubProfile));
    mockServerClient
        .when(
            request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH + stubProfile),
            Times.exactly(3))
        .respond(response().withStatusCode(501).withBody("failure"));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH + stubProfile))
        .respond(response().withBody(STUB_CREDENTIALS));

    resolveCredentials();

    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH + stubProfile)
            .withHeader(TOKEN_HEADER, stubToken),
        VerificationTimes.exactly(4));
  }

  @Test
  public void resolveCredentials_tooManyRetries() {
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withBody(stubToken));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH))
        .respond(response().withBody(stubProfile));
    mockServerClient
        .when(
            request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH + stubProfile),
            Times.exactly(4))
        .respond(response().withStatusCode(501).withBody("failure"));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH + stubProfile))
        .respond(response().withBody(STUB_CREDENTIALS));

    assertThrows(SdkClientException.class, this::resolveCredentials);

    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH + stubProfile)
            .withHeader(TOKEN_HEADER, stubToken),
        VerificationTimes.exactly(4));
  }

  @Test
  public void resolveCredentials_queriesTokenResource_403Error_fallbackToInsecure() {
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withStatusCode(403).withBody("failure"));
    mockServerClient
        .when(
            request()
                .withMethod("GET")
                .withPath(CREDENTIALS_RESOURCE_PATH)
                .withHeader(Header.header(NottableString.not(TOKEN_HEADER))))
        .respond(response().withBody(stubProfile));
    mockServerClient
        .when(
            request()
                .withMethod("GET")
                .withPath(CREDENTIALS_RESOURCE_PATH + stubProfile)
                .withHeader(Header.header(NottableString.not(TOKEN_HEADER))))
        .respond(response().withBody(STUB_CREDENTIALS));

    resolveCredentials();

    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH)
            .withHeader(Header.header(NottableString.not("x-aws-ec2-metadata-token"))),
        VerificationTimes.exactly(1));
    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH + stubProfile)
            .withHeader(Header.header(NottableString.not("x-aws-ec2-metadata-token"))),
        VerificationTimes.exactly(1));
  }

  @Test
  public void resolveCredentials_queriesTokenResource_404Error_fallbackToInsecure() {
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withStatusCode(404).withBody("failure"));
    mockServerClient
        .when(
            request()
                .withMethod("GET")
                .withPath(CREDENTIALS_RESOURCE_PATH)
                .withHeader(Header.header(NottableString.not(TOKEN_HEADER))))
        .respond(response().withBody(stubProfile));
    mockServerClient
        .when(
            request()
                .withMethod("GET")
                .withPath(CREDENTIALS_RESOURCE_PATH + stubProfile)
                .withHeader(Header.header(NottableString.not(TOKEN_HEADER))))
        .respond(response().withBody(STUB_CREDENTIALS));

    resolveCredentials();

    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH)
            .withHeader(Header.header(NottableString.not("x-aws-ec2-metadata-token"))),
        VerificationTimes.exactly(1));
    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH + stubProfile)
            .withHeader(Header.header(NottableString.not("x-aws-ec2-metadata-token"))),
        VerificationTimes.exactly(1));
  }

  @Test
  public void resolveCredentials_queriesTokenResource_405Error_fallbackToInsecure() {
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withStatusCode(405).withBody("failure"));
    mockServerClient
        .when(
            request()
                .withMethod("GET")
                .withPath(CREDENTIALS_RESOURCE_PATH)
                .withHeader(Header.header(NottableString.not(TOKEN_HEADER))))
        .respond(response().withBody(stubProfile));
    mockServerClient
        .when(
            request()
                .withMethod("GET")
                .withPath(CREDENTIALS_RESOURCE_PATH + stubProfile)
                .withHeader(Header.header(NottableString.not(TOKEN_HEADER))))
        .respond(response().withBody(STUB_CREDENTIALS));

    resolveCredentials();

    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH)
            .withHeader(Header.header(NottableString.not(TOKEN_HEADER))),
        VerificationTimes.exactly(1));
    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH + stubProfile)
            .withHeader(Header.header(NottableString.not(TOKEN_HEADER))),
        VerificationTimes.exactly(1));
  }

  @Test
  public void resolveCredentials_queriesTokenResource_400Error_throws() {
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withStatusCode(400).withBody("failure"));

    InstanceProfileCredentialsWithRetryProvider provider =
        InstanceProfileCredentialsWithRetryProvider.builder()
            .endpoint("http://localhost:" + mockServerClient.getPort())
            .retryConfig(retryConfig)
            .build();

    SdkClientException ex = assertThrows(SdkClientException.class, provider::resolveCredentials);
    assertThat(ex.getMessage()).startsWith("Failed to load credentials from IMDS.");
  }

  @Test
  public void resolveCredentials_queriesTokenResource_socketTimeout_fallbackToInsecure() {
    Delay delay = new Delay(TimeUnit.MILLISECONDS, Integer.MAX_VALUE);
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withBody(stubToken).withDelay(delay));
    mockServerClient
        .when(
            request()
                .withMethod("GET")
                .withPath(CREDENTIALS_RESOURCE_PATH)
                .withHeader(Header.header(NottableString.not(TOKEN_HEADER))))
        .respond(response().withBody(stubProfile));
    mockServerClient
        .when(
            request()
                .withMethod("GET")
                .withPath(CREDENTIALS_RESOURCE_PATH + stubProfile)
                .withHeader(Header.header(NottableString.not(TOKEN_HEADER))))
        .respond(response().withBody(STUB_CREDENTIALS));

    resolveCredentials();

    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH)
            .withHeader(Header.header(NottableString.not(TOKEN_HEADER))),
        VerificationTimes.exactly(1));
    mockServerClient.verify(
        request()
            .withMethod("GET")
            .withPath(CREDENTIALS_RESOURCE_PATH + stubProfile)
            .withHeader(Header.header(NottableString.not(TOKEN_HEADER))),
        VerificationTimes.exactly(1));
  }

  @Test
  public void resolveCredentials_doesNotFailIfImdsReturnsExpiredCredentials() {
    String expiredCredentials =
        "{"
            + "\"AccessKeyId\":\"ACCESS_KEY_ID\","
            + "\"SecretAccessKey\":\"SECRET_ACCESS_KEY\","
            + "\"Expiration\":\""
            + DateUtils.formatIso8601Date(Instant.now().minus(Duration.ofHours(1)))
            + '"'
            + "}";
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withBody(stubToken));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH))
        .respond(response().withBody(stubProfile));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH + stubProfile))
        .respond(response().withBody(expiredCredentials));

    AwsCredentials creds = resolveCredentials();

    assertThat(creds.accessKeyId()).isEqualTo("ACCESS_KEY_ID");
    assertThat(creds.secretAccessKey()).isEqualTo("SECRET_ACCESS_KEY");
  }

  @Test
  public void resolveCredentials_onlyCallsImdsOnceEvenWithExpiredCredentials() {
    String credentialsResponse =
        "{"
            + "\"AccessKeyId\":\"ACCESS_KEY_ID\","
            + "\"SecretAccessKey\":\"SECRET_ACCESS_KEY\","
            + "\"Expiration\":\""
            + DateUtils.formatIso8601Date(Instant.now().minus(Duration.ofHours(1)))
            + '"'
            + "}";
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withBody(stubToken));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH))
        .respond(response().withBody(stubProfile));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH + stubProfile))
        .respond(response().withBody(credentialsResponse));
    InstanceProfileCredentialsWithRetryProvider provider =
        InstanceProfileCredentialsWithRetryProvider.builder()
            .endpoint("http://localhost:" + mockServerClient.getPort())
            .retryConfig(retryConfig)
            .build();

    provider.resolveCredentials();

    // "once" in this sense includes all the retries for one attempt
    mockServerClient.verify(
        request().withPath(CREDENTIALS_RESOURCE_PATH + stubProfile), VerificationTimes.exactly(1));

    provider.resolveCredentials();
    provider.resolveCredentials();

    mockServerClient.verify(
        request().withPath(CREDENTIALS_RESOURCE_PATH + stubProfile), VerificationTimes.exactly(1));
  }

  @Test
  public void resolveCredentials_failsIfImdsReturns500OnFirstCall() {
    String errorMessage = "XXXXX";
    String credentialsResponse =
        "{"
            + "\"code\": \"InternalServiceException\","
            + "\"message\": \""
            + errorMessage
            + "\""
            + "}";
    mockServerClient
        .when(request().withMethod("PUT").withPath(TOKEN_RESOURCE_PATH))
        .respond(response().withBody(stubToken));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH))
        .respond(response().withBody(stubProfile));
    mockServerClient
        .when(request().withMethod("GET").withPath(CREDENTIALS_RESOURCE_PATH + stubProfile))
        .respond(response().withStatusCode(500).withBody(credentialsResponse));

    Exception ex = assertThrows(SdkClientException.class, this::resolveCredentials);

    assertThat(ex.getCause().getMessage())
        .startsWith("Failed to load credentials from metadata service");
  }
}
