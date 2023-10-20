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

import com.google.common.primitives.Ints;
import com.google.inject.Inject;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetEncryptedPrivateKeyResponseProto.GetEncryptedPrivateKeyResponse;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.util.ErrorUtil;
import com.google.scp.shared.api.util.HttpClientWrapper;
import java.io.IOException;
import java.net.URI;
import java.time.Duration;
import org.apache.http.client.config.RequestConfig;
import org.apache.http.client.methods.HttpGet;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Client which fetches encrypted private keys from the private key vending service. */
public final class HttpEncryptionKeyFetchingService implements EncryptionKeyFetchingService {

  private static final int REQUEST_TIMEOUT_DURATION =
      Ints.checkedCast(Duration.ofMinutes(1).toMillis());
  private final HttpClientWrapper httpClient;
  private final String encryptionKeyServiceBaseUrl;
  private final Logger logger = LoggerFactory.getLogger(HttpEncryptionKeyFetchingService.class);

  /** Creates a new instance of the {@code HttpPrivateKeyFetchingService} class. */
  @Inject
  public HttpEncryptionKeyFetchingService(
      HttpClientWrapper httpClient, String encryptionKeyServiceBaseUrl) {
    this.httpClient = httpClient;
    this.encryptionKeyServiceBaseUrl = encryptionKeyServiceBaseUrl;
  }

  /**
   * Make a request to split key vending service for specified key id at specified base URL and
   * returns {@link EncryptionKey}. Errors are wrapped in a {@link
   * EncryptionKeyFetchingServiceException}.
   */
  @Override
  public EncryptionKey fetchEncryptionKey(String keyId)
      throws EncryptionKeyFetchingServiceException {
    URI fetchUri =
        URI.create(String.format("%s/encryptionKeys/%s", encryptionKeyServiceBaseUrl, keyId));
    var request = new HttpGet(fetchUri);

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

    try {
      var response = httpClient.execute(request);
      var responseBody = response.responseBody();

      if (response.statusCode() != 200) {
        var errorResponse = ErrorUtil.parseErrorResponse(responseBody);
        var exception = ErrorUtil.toServiceException(errorResponse);

        var message = "Received error from private key vending service";
        logger.error(message, exception);
        throw new EncryptionKeyFetchingServiceException(message, exception);
      } else {
        logger.info(
            "Successfully fetched encrypted key-split for keyId: "
                + keyId
                + " using Uri: "
                + fetchUri);
      }

      return parseSuccessResponse(responseBody);
    } catch (IOException e) {
      var message = "Error fetching private key ciphertext";
      logger.error(message, e);
      throw new EncryptionKeyFetchingServiceException(message, e);
    }
  }

  /**
   * Attempts to read the body of a 200 response and convert it to a {@link
   * GetEncryptedPrivateKeyResponse}, wrapping parsing errors in a {@link
   * EncryptionKeyFetchingServiceException}
   */
  private EncryptionKey parseSuccessResponse(String responseBody)
      throws EncryptionKeyFetchingServiceException {
    try {
      EncryptionKey.Builder builder = EncryptionKey.newBuilder();
      JsonFormat.parser().ignoringUnknownFields().merge(responseBody, builder);
      return builder.build();
    } catch (InvalidProtocolBufferException e) {
      var message = "Failed to parse success response as EncryptedPrivateKey";
      logger.error(message, e);
      throw new EncryptionKeyFetchingServiceException(message, e);
    }
  }
}
