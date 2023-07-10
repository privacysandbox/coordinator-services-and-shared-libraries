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

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.common.collect.ImmutableList;
import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import org.apache.hc.core5.http.HttpEntityContainer;
import org.apache.http.HttpEntityEnclosingRequest;
import org.apache.http.HttpRequest;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.StringEntity;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import software.amazon.awssdk.auth.credentials.AwsBasicCredentials;
import software.amazon.awssdk.auth.credentials.AwsCredentials;
import software.amazon.awssdk.regions.Region;

/** Tests for the utility class that signs Apache HttpRequest objects for AWS. */
@RunWith(JUnit4.class)
public class AwsRequestSignerTest {
  @Test
  public void makeSignedHttpRequest_successOnPort443() throws URISyntaxException, IOException {
    String service = "execute-api";
    Region region = Region.US_WEST_2;
    AwsCredentials creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");

    int port = 443;
    HttpRequest inputRequest = new HttpGet("https://www.google.com:" + port);
    HttpRequest outputRequest =
        AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds);

    assertThat(inputRequest.containsHeader("Authorization")).isFalse();
    assertThat(inputRequest.containsHeader("X-Amz-Date")).isFalse();
    assertThat(outputRequest.containsHeader("Authorization")).isTrue();
    assertThat(outputRequest.containsHeader("X-Amz-Date")).isTrue();

    // Expected, Apache request objects remove default ports
    assertThat(new URI(outputRequest.getRequestLine().getUri()).getPort()).isEqualTo(-1);
  }

  @Test
  public void makeSignedHttpRequest_successOnOtherPort() throws URISyntaxException, IOException {
    String service = "execute-api";
    Region region = Region.US_WEST_2;
    AwsCredentials creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");

    int port = 8080;
    HttpRequest inputRequest = new HttpGet("https://www.google.com:" + port);
    HttpRequest outputRequest =
        AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds);

    assertThat(inputRequest.containsHeader("Authorization")).isFalse();
    assertThat(inputRequest.containsHeader("X-Amz-Date")).isFalse();
    assertThat(outputRequest.containsHeader("Authorization")).isTrue();
    assertThat(outputRequest.containsHeader("X-Amz-Date")).isTrue();

    // Expected, Apache includes port only when not equal to the default
    assertThat(new URI(outputRequest.getRequestLine().getUri()).getPort()).isEqualTo(port);
  }

  @Test
  public void makeSignedHttpRequest_retainsHeaders() throws URISyntaxException, IOException {
    var service = "execute-api";
    var region = Region.US_WEST_2;
    var creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");
    var inputRequest = new HttpGet("https://www.google.com");
    inputRequest.addHeader("content-type", "application/json");
    inputRequest.addHeader("my-custom-header", "foo-bar");

    var outputRequest =
        AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds);

    assertThat(outputRequest.getFirstHeader("content-type").getValue())
        .isEqualTo("application/json");
    assertThat(outputRequest.getFirstHeader("my-custom-header").getValue()).isEqualTo("foo-bar");
  }

  @Test
  public void makeSignedHttpRequest_multiheadertest() throws URISyntaxException, IOException {
    var service = "execute-api";
    var region = Region.US_WEST_2;
    var creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");
    var inputRequest = new HttpGet("https://www.google.com");
    inputRequest.addHeader("Set-Cookie", "foo");
    inputRequest.addHeader("Set-Cookie", "bar");
    inputRequest.addHeader("Set-Cookie", "baz");

    var outputRequest =
        AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds);

    var headers = ImmutableList.copyOf(outputRequest.getHeaders("Set-Cookie"));
    assertThat(headers.stream().map(header -> header.getValue()).collect(toImmutableList()))
        .isEqualTo(ImmutableList.of("foo", "bar", "baz"));
  }

  @Test
  public void makeSignedHttpRequest_nullService() {
    String service = null;
    Region region = Region.US_WEST_2;
    AwsCredentials creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");

    int port = 8080;
    HttpRequest inputRequest = new HttpGet("https://www.google.com:" + port);

    assertThrows(
        NullPointerException.class,
        () -> AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds));
  }

  @Test
  public void makeSignedHttpRequest_nullRegion() {
    String service = "execute-api";
    Region region = null;
    AwsCredentials creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");

    int port = 8080;
    HttpRequest inputRequest = new HttpGet("https://www.google.com:" + port);

    assertThrows(
        NullPointerException.class,
        () -> AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds));
  }

  @Test
  public void makeSignedHttpRequest_nullRequest() {
    String service = "execute-api";
    Region region = Region.US_WEST_2;
    AwsCredentials creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");

    HttpRequest inputRequest = null;

    assertThrows(
        NullPointerException.class,
        () -> AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds));
  }

  @Test
  public void makeSignedHttpRequest_nullCredentials() {
    String service = "execute-api";
    Region region = Region.US_WEST_2;
    AwsCredentials creds = null;

    int port = 8080;
    HttpRequest inputRequest = new HttpGet("https://www.google.com:" + port);

    assertThrows(
        NullPointerException.class,
        () -> AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds));
  }

  @Test
  public void makeSignedHttpRequest_includesBody() throws Exception {
    var service = "execute-api";
    var region = Region.US_WEST_2;
    var inputContent = "{\"foo\": \"bar\"}";
    var creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");
    var inputRequest = new HttpPost("https://www.google.com");
    inputRequest.setEntity(new StringEntity(inputContent));

    var outputRequest =
        AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds);

    var outputEntity = ((HttpEntityEnclosingRequest) outputRequest).getEntity();
    assertThat(new String(outputEntity.getContent().readAllBytes())).isEqualTo(inputContent);
    assertThat(outputRequest.getRequestLine().toString())
        .isEqualTo("POST https://www.google.com HTTP/1.1");
  }

  // The following tests verify the same behaviour as above but for methods dealing with Apache
  // HttpClient v5
  @Test
  public void makeSignedHttpRequest_successOnPort443V2() throws URISyntaxException, IOException {
    String service = "execute-api";
    Region region = Region.US_WEST_2;
    AwsCredentials creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");

    int port = 443;
    org.apache.hc.core5.http.HttpRequest inputRequest =
        new org.apache.hc.client5.http.classic.methods.HttpGet("https://www.google.com:" + port);
    org.apache.hc.core5.http.HttpRequest outputRequest =
        AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds);

    assertThat(inputRequest.containsHeader("Authorization")).isFalse();
    assertThat(inputRequest.containsHeader("X-Amz-Date")).isFalse();
    assertThat(outputRequest.containsHeader("Authorization")).isTrue();
    assertThat(outputRequest.containsHeader("X-Amz-Date")).isTrue();

    // Expected, Apache request objects remove default ports
    assertThat(outputRequest.getUri().getPort()).isEqualTo(-1);
  }

  @Test
  public void makeSignedHttpRequest_successOnOtherPortV2() throws URISyntaxException, IOException {
    String service = "execute-api";
    Region region = Region.US_WEST_2;
    AwsCredentials creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");

    int port = 8080;
    org.apache.hc.core5.http.HttpRequest inputRequest =
        new org.apache.hc.client5.http.classic.methods.HttpGet("https://www.google.com:" + port);
    org.apache.hc.core5.http.HttpRequest outputRequest =
        AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds);

    assertThat(inputRequest.containsHeader("Authorization")).isFalse();
    assertThat(inputRequest.containsHeader("X-Amz-Date")).isFalse();
    assertThat(outputRequest.containsHeader("Authorization")).isTrue();
    assertThat(outputRequest.containsHeader("X-Amz-Date")).isTrue();

    // Expected, Apache includes port only when not equal to the default
    assertThat(outputRequest.getUri().getPort()).isEqualTo(port);
  }

  @Test
  public void makeSignedHttpRequest_retainsHeadersV2() throws URISyntaxException, IOException {
    var service = "execute-api";
    var region = Region.US_WEST_2;
    var creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");
    var inputRequest =
        new org.apache.hc.client5.http.classic.methods.HttpGet("https://www.google.com");
    inputRequest.addHeader("content-type", "application/json");
    inputRequest.addHeader("my-custom-header", "foo-bar");

    var outputRequest =
        AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds);

    assertThat(outputRequest.getFirstHeader("content-type").getValue())
        .isEqualTo("application/json");
    assertThat(outputRequest.getFirstHeader("my-custom-header").getValue()).isEqualTo("foo-bar");
  }

  @Test
  public void makeSignedHttpRequest_multiheadertestV2() throws URISyntaxException, IOException {
    var service = "execute-api";
    var region = Region.US_WEST_2;
    var creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");
    var inputRequest =
        new org.apache.hc.client5.http.classic.methods.HttpGet("https://www.google.com");
    inputRequest.addHeader("Set-Cookie", "foo");
    inputRequest.addHeader("Set-Cookie", "bar");
    inputRequest.addHeader("Set-Cookie", "baz");

    var outputRequest =
        AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds);

    var headers = ImmutableList.copyOf(outputRequest.getHeaders("Set-Cookie"));
    assertThat(headers.stream().map(header -> header.getValue()).collect(toImmutableList()))
        .isEqualTo(ImmutableList.of("foo", "bar", "baz"));
  }

  @Test
  public void makeSignedHttpRequest_nullServiceV2() {
    String service = null;
    Region region = Region.US_WEST_2;
    AwsCredentials creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");

    int port = 8080;
    org.apache.hc.core5.http.HttpRequest inputRequest =
        new org.apache.hc.client5.http.classic.methods.HttpGet("https://www.google.com:" + port);

    assertThrows(
        NullPointerException.class,
        () -> AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds));
  }

  @Test
  public void makeSignedHttpRequest_nullRegionV2() {
    String service = "execute-api";
    Region region = null;
    AwsCredentials creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");

    int port = 8080;
    org.apache.hc.core5.http.HttpRequest inputRequest =
        new org.apache.hc.client5.http.classic.methods.HttpGet("https://www.google.com:" + port);

    assertThrows(
        NullPointerException.class,
        () -> AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds));
  }

  @Test
  public void makeSignedHttpRequest_nullRequestV2() {
    String service = "execute-api";
    Region region = Region.US_WEST_2;
    AwsCredentials creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");

    org.apache.hc.core5.http.HttpRequest inputRequest = null;

    assertThrows(
        NullPointerException.class,
        () -> AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds));
  }

  @Test
  public void makeSignedHttpRequest_nullCredentialsV2() {
    String service = "execute-api";
    Region region = Region.US_WEST_2;
    AwsCredentials creds = null;

    int port = 8080;
    org.apache.hc.core5.http.HttpRequest inputRequest =
        new org.apache.hc.client5.http.classic.methods.HttpGet("https://www.google.com:" + port);

    assertThrows(
        NullPointerException.class,
        () -> AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds));
  }

  @Test
  public void makeSignedHttpRequest_includesBodyV2() throws Exception {
    var service = "execute-api";
    var region = Region.US_WEST_2;
    var inputContent = "{\"foo\": \"bar\"}";
    var creds = AwsBasicCredentials.create("accessKeyId", "secretAccessKey");
    var inputRequest =
        new org.apache.hc.client5.http.classic.methods.HttpPost("https://www.google.com");
    inputRequest.setEntity(new org.apache.hc.core5.http.io.entity.StringEntity(inputContent));

    var outputRequest =
        AwsRequestSigner.makeSignedHttpRequest(inputRequest, service, region, creds);

    var outputEntity = ((HttpEntityContainer) outputRequest).getEntity();
    assertThat(new String(outputEntity.getContent().readAllBytes())).isEqualTo(inputContent);
    assertThat(outputRequest.toString()).isEqualTo("POST https://www.google.com/");
  }
}
