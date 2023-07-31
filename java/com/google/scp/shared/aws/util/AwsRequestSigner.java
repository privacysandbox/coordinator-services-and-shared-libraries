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

import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Optional;
import org.apache.hc.core5.http.HttpEntity;
import org.apache.hc.core5.http.HttpEntityContainer;
import org.apache.hc.core5.http.io.support.ClassicRequestBuilder;
import org.apache.http.HttpEntityEnclosingRequest;
import org.apache.http.HttpException;
import org.apache.http.HttpHost;
import org.apache.http.HttpRequest;
import org.apache.http.HttpRequestInterceptor;
import org.apache.http.client.methods.RequestBuilder;
import org.apache.http.entity.StringEntity;
import org.apache.http.protocol.HttpCoreContext;
import software.amazon.awssdk.auth.credentials.AwsCredentials;
import software.amazon.awssdk.auth.signer.Aws4Signer;
import software.amazon.awssdk.auth.signer.params.Aws4SignerParams;
import software.amazon.awssdk.http.SdkHttpFullRequest;
import software.amazon.awssdk.http.SdkHttpMethod;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.utils.StringInputStream;

/**
 * A utility class for signing Apache HttpRequest objects with AWS v4 signatures. Currently, this
 * class supports both Apache Httpclient v4 and v5. The support for v4 can be removed once the
 * entire repo has been updated to consume Apache httpclient v5
 */
public final class AwsRequestSigner {
  private AwsRequestSigner() {}
  /**
   * Clones an Apache HTTP request using the AWS signature V4 process, adding AWS headers.
   *
   * @param request The request to be signed. The signature overrides the provided port with 443.
   * @param serviceName The AWS service name being accessed, e.g. "execute-api".
   * @param region The AWS region in which the service is being accessed.
   * @param credentials The AWS credentials being used to generate the signature.
   */
  public static HttpRequest makeSignedHttpRequest(
      HttpRequest request, String serviceName, Region region, AwsCredentials credentials)
      throws URISyntaxException, IOException {
    // Make an explicit copy of the body to ensure that the internal InputStream isn't consumed when
    // translating between different types (there is probably a cleaner way of doing this).
    var body = extractBody(request);
    var sdkRequest = toSdkHttpRequest(request, body);
    var signedSdkRequest = signHttpRequest(sdkRequest, serviceName, region, credentials);
    return toApacheHttpRequest(signedSdkRequest, body);
  }

  /**
   * Clones an Apache HTTP request using the AWS signature V4 process, adding AWS headers.
   *
   * @param request The request to be signed. The signature overrides the provided port with 443.
   * @param serviceName The AWS service name being accessed, e.g. "execute-api".
   * @param region The AWS region in which the service is being accessed.
   * @param credentials The AWS credentials being used to generate the signature.
   */
  public static org.apache.hc.core5.http.HttpRequest makeSignedHttpRequest(
      org.apache.hc.core5.http.HttpRequest request,
      String serviceName,
      Region region,
      AwsCredentials credentials)
      throws URISyntaxException, IOException {
    // Make an explicit copy of the body to ensure that the internal InputStream isn't consumed when
    // translating between different types (there is probably a cleaner way of doing this).
    var body = extractBody(request);
    var sdkRequest = toSdkHttpRequest(request, body);
    var signedSdkRequest = signHttpRequest(sdkRequest, serviceName, region, credentials);
    return toApacheHttpRequestV2(signedSdkRequest, body);
  }

  /**
   * Returns an Apache HttpClient interceptor which adds AWS authentication headers to the request
   * using the AWS v4 signature method.
   *
   * <p>Assumes that the request body (if any) can be converted to a StringEntity.
   */
  public static HttpRequestInterceptor createRequestSignerInterceptor(
      Region region, AwsSessionCredentialsProvider credentials) {
    return (request, context) -> {
      try {
        // The request doesn't have the full URI for some reason, so it is rebuilt here
        String uriWithoutPath =
            ((HttpHost) context.getAttribute(HttpCoreContext.HTTP_TARGET_HOST)).toString();
        String pathWithoutHost = request.getRequestLine().getUri();
        String fullUri = uriWithoutPath + pathWithoutHost;
        HttpRequest tempRequest = RequestBuilder.copy(request).setUri(fullUri).build();
        tempRequest =
            AwsRequestSigner.makeSignedHttpRequest(
                tempRequest, "execute-api", region, credentials.resolveCredentials());
        request.setHeaders(tempRequest.getAllHeaders());
      } catch (URISyntaxException exception) {
        throw new HttpException("Syntax error in URI.", exception);
      }
    };
  }

  private static SdkHttpFullRequest changePort(SdkHttpFullRequest request, int port) {
    return request.toBuilder().port(port).build();
  }
  /**
   * Consumes the input stream on the provided HTTP request (if it exists) and returns it as a
   * string. Returns empty Optional if there is no body associated with the request.
   */
  private static Optional<String> extractBody(HttpRequest request) throws IOException {
    if (request instanceof HttpEntityEnclosingRequest) {
      var inputStream = ((HttpEntityEnclosingRequest) request).getEntity().getContent();
      return Optional.of(new String(inputStream.readAllBytes()));
    }
    return Optional.empty();
  }

  /**
   * Consumes the input stream on the provided HTTP request (if it exists) and returns it as a
   * string. Returns empty Optional if there is no body associated with the request.
   */
  private static Optional<String> extractBody(org.apache.hc.core5.http.HttpRequest request)
      throws IOException {
    if (request instanceof HttpEntityContainer) {
      Optional<HttpEntity> entity =
          Optional.ofNullable(((HttpEntityContainer) request).getEntity());
      if (entity.isPresent()) {
        var inputStream = entity.get().getContent();
        return Optional.of(new String(inputStream.readAllBytes()));
      }
    }
    return Optional.empty();
  }

  /**
   * Reads the URI, method, and headers of the provided AWS HTTP request and generates a new Apache
   * HTTP Request.
   */
  private static HttpRequest toApacheHttpRequest(SdkHttpFullRequest request, Optional<String> body)
      throws IOException {
    var builder = RequestBuilder.create(request.method().toString());

    builder.setUri(request.getUri().toString());

    for (var header : request.headers().entrySet()) {
      // Handle corner-case where header may have multiple values and add each of them to the
      // request (generally calling code should not do this though).
      for (var value : header.getValue()) {
        builder.addHeader(header.getKey(), value);
      }
    }

    if (body.isPresent()) {
      var entity = new StringEntity(body.get());
      builder.setEntity(entity);
    }
    return builder.build();
  }

  /**
   * Reads the URI, method, and headers of the provided AWS HTTP request and generates a new Apache
   * HTTP Request.
   */
  private static org.apache.hc.core5.http.HttpRequest toApacheHttpRequestV2(
      SdkHttpFullRequest request, Optional<String> body) {
    var builder = ClassicRequestBuilder.create(request.method().toString());

    builder.setUri(request.getUri().toString());

    for (var header : request.headers().entrySet()) {
      // Handle corner-case where header may have multiple values and add each of them to the
      // request (generally calling code should not do this though).
      for (var value : header.getValue()) {
        builder.addHeader(header.getKey(), value);
      }
    }

    if (body.isPresent()) {
      builder.setEntity(body.get());
    }

    return builder.build();
  }

  /**
   * Reads the URI, method, and headers of the provided Apache HTTP request and generates a new AWS
   * SDK request.
   *
   * @param body The body to add to the generated request (note: the body is necessary for
   *     generating the auth signature)
   */
  private static SdkHttpFullRequest toSdkHttpRequest(HttpRequest request, Optional<String> body)
      throws URISyntaxException {
    var builder =
        SdkHttpFullRequest.builder()
            .uri(new URI(request.getRequestLine().getUri()))
            .method(SdkHttpMethod.fromValue(request.getRequestLine().getMethod()));
    for (var header : request.getAllHeaders()) {
      builder.appendHeader(header.getName(), header.getValue());
    }
    body.ifPresent(b -> builder.contentStreamProvider(() -> new StringInputStream(b)));
    return builder.build();
  }

  /**
   * Reads the URI, method, and headers of the provided Apache HTTP request and generates a new AWS
   * SDK request.
   *
   * @param body The body to add to the generated request (note: the body is necessary for
   *     generating the auth signature)
   */
  private static SdkHttpFullRequest toSdkHttpRequest(
      org.apache.hc.core5.http.HttpRequest request, Optional<String> body)
      throws URISyntaxException {
    var builder =
        SdkHttpFullRequest.builder()
            .uri(request.getUri())
            .method(SdkHttpMethod.fromValue(request.getMethod()));

    for (var header : request.getHeaders()) {
      builder.appendHeader(header.getName(), header.getValue());
    }

    body.ifPresent(b -> builder.contentStreamProvider(() -> new StringInputStream(b)));

    return builder.build();
  }

  private static SdkHttpFullRequest signHttpRequest(
      SdkHttpFullRequest request, String serviceName, Region region, AwsCredentials credentials) {
    var originalPort = request.getUri().getPort();

    // TODO: A future proxy update will remove the need to adjust the port here
    // AWS will see the request on port 443, so the request should be signed accordingly
    var externalRequest = changePort(request, 443);

    var sdkRequestParams =
        Aws4SignerParams.builder()
            .signingName(serviceName)
            .signingRegion(region)
            .awsCredentials(credentials)
            .doubleUrlEncode(true)
            .build();

    externalRequest = Aws4Signer.create().sign(externalRequest, sdkRequestParams);

    // TODO: A future proxy update will remove the need to adjust the port here
    // Restore original port for outgoing request
    var updatedRequest = changePort(externalRequest, originalPort);

    return updatedRequest;
  }
}
