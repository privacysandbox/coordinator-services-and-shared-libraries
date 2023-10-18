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

package com.google.scp.operator.cpio.cryptoclient;

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.common.primitives.Ints;
import com.google.inject.BindingAnnotation;
import com.google.inject.Inject;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetEncryptedPrivateKeyResponseProto.GetEncryptedPrivateKeyResponse;
import com.google.scp.shared.api.util.ErrorUtil;
import com.google.scp.shared.api.util.HttpClientWrapper;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;
import java.net.URI;
import java.time.Duration;
import org.apache.http.client.config.RequestConfig;
import org.apache.http.client.methods.HttpGet;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Client which fetches encrypted private keys from the private key vending service.
 *
 * @deprecated Single-party key features are deprecated. Pending removal b/282204533.
 */
@Deprecated
public final class HttpPrivateKeyFetchingService implements PrivateKeyFetchingService {

  private static final int REQUEST_TIMEOUT_DURATION =
      Ints.checkedCast(Duration.ofMinutes(1).toMillis());
  // Base URL (e.g. `https://foo.com/v1`).
  private final String privateKeyServiceBaseUrl;
  private final HttpClientWrapper httpClient;
  private final Logger logger = LoggerFactory.getLogger(HttpPrivateKeyFetchingService.class);

  /** Creates a new instance of the {@code HttpPrivateKeyFetchingService} class. */
  @Inject
  public HttpPrivateKeyFetchingService(
      @PrivateKeyServiceBaseUrl String privateKeyServiceBaseUrl, HttpClientWrapper httpClient) {
    this.privateKeyServiceBaseUrl = privateKeyServiceBaseUrl;
    this.httpClient = httpClient;
  }

  @Override
  public String fetchKeyCiphertext(String keyId) throws PrivateKeyFetchingServiceException {
    try {
      var request = new HttpGet(getFetchUri(keyId));

      final RequestConfig requestConfig =
          RequestConfig.custom()
              // Timeout for requesting a connection from internal connection manager
              .setConnectionRequestTimeout(REQUEST_TIMEOUT_DURATION)
              // Timeout for establishing a request to host
              .setConnectTimeout(REQUEST_TIMEOUT_DURATION)
              // Timeout between data packets received
              .setSocketTimeout(REQUEST_TIMEOUT_DURATION)
              .build();
      request.setConfig(requestConfig);

      var response = httpClient.execute(request);
      var responseBody = response.responseBody();

      if (response.statusCode() != 200) {
        var errorResponse = ErrorUtil.parseErrorResponse(responseBody);
        var exception = ErrorUtil.toServiceException(errorResponse);

        var message = "Received error from private key vending service";
        logger.error(message, exception);
        throw new PrivateKeyFetchingServiceException(message, exception);
      }

      var encryptedPrivateKey = parseSuccessResponse(responseBody);
      return encryptedPrivateKey.getJsonEncodedKeyset();
    } catch (IOException | IllegalArgumentException e) {
      var message = "Error fetching private key ciphertext";
      logger.error(message, e);
      throw new PrivateKeyFetchingServiceException(message, e);
    }
  }

  private URI getFetchUri(String keyId) {
    return URI.create(String.format("%s/privateKeys/%s", privateKeyServiceBaseUrl, keyId));
  }

  /**
   * Attempts to read the body of a 200 response and convert it to a {@link
   * GetEncryptedPrivateKeyResponse}, wrapping parsing errors in a {@link
   * PrivateKeyFetchingServiceException}
   */
  private GetEncryptedPrivateKeyResponse parseSuccessResponse(String responseBody)
      throws PrivateKeyFetchingServiceException {
    try {
      GetEncryptedPrivateKeyResponse.Builder builder = GetEncryptedPrivateKeyResponse.newBuilder();
      JsonFormat.parser().ignoringUnknownFields().merge(responseBody, builder);
      return builder.build();
    } catch (InvalidProtocolBufferException e) {
      var message = "Failed to parse success response as EncryptedPrivateKey";
      logger.error(message, e);
      throw new PrivateKeyFetchingServiceException(message, e);
    }
  }

  /** Base URL (e.g. `https://foo.com/v1`) where the private key vending service is located. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PrivateKeyServiceBaseUrl {}
}
