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

package com.google.scp.coordinator.keymanagement.keygeneration.app.common;

import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.GetDataKeyBaseUrlOverride;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyStorageServiceBaseUrl;

import com.google.common.primitives.Ints;
import com.google.inject.Inject;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.CoordinatorBHttpClient;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.converter.DataKeyConverter;
import com.google.scp.coordinator.keymanagement.keystorage.converters.EncryptionKeyConverter;
import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.CreateKeyRequestProto.CreateKeyRequest;
import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.DataKeyProto;
import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.KeySplitEncryptionTypeProto.KeySplitEncryptionType;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.util.ErrorUtil;
import java.io.IOException;
import java.net.URI;
import java.time.Duration;
import java.util.Optional;
import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.client.HttpClient;
import org.apache.http.client.config.RequestConfig;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.ContentType;
import org.apache.http.entity.StringEntity;
import org.apache.http.util.EntityUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * HTTP Client to communicate with the Key Storage Service to send and store {@link EncryptionKey}
 * key splits and receive signatures in response.
 */
public final class HttpKeyStorageClient implements KeyStorageClient {

  private static final int REQUEST_TIMEOUT_DURATION =
      Ints.checkedCast(Duration.ofMinutes(1).toMillis());
  private static final RequestConfig REQUEST_CONFIG =
      RequestConfig.custom()
          // Timeout for requesting a connection from internal connection manager
          .setConnectionRequestTimeout(REQUEST_TIMEOUT_DURATION)
          // Timeout for establishing a request to host
          .setConnectTimeout(REQUEST_TIMEOUT_DURATION)
          // Timeout between data packets received
          .setSocketTimeout(REQUEST_TIMEOUT_DURATION)
          .build();
  // Base URL (e.g. `https://foo.com/v1`).
  private final String createKeyBaseUrl;
  private final String getDataKeyBaseUrl;
  private final HttpClient httpClient;
  private final Logger logger = LoggerFactory.getLogger(HttpKeyStorageClient.class);

  /** Creates a new instance of the {@code HttpKeyStorageClient} class. */
  @Inject
  public HttpKeyStorageClient(
      @KeyStorageServiceBaseUrl String keyStorageServiceBaseUrl,
      @CoordinatorBHttpClient HttpClient httpClient,
      @GetDataKeyBaseUrlOverride Optional<String> getDataKeyBaseUrlOverride) {
    this.createKeyBaseUrl = keyStorageServiceBaseUrl;
    this.getDataKeyBaseUrl = getDataKeyBaseUrlOverride.orElse(keyStorageServiceBaseUrl);
    this.httpClient = httpClient;
  }

  @Override
  public EncryptionKey createKey(EncryptionKey encryptionKey, String encryptedKeySplit)
      throws KeyStorageServiceException {
    var apiKey = EncryptionKeyConverter.toApiEncryptionKey(encryptionKey);

    var createKeyRequest =
        CreateKeyRequest.newBuilder()
            .setKeyId(encryptionKey.getKeyId())
            .setKey(apiKey)
            .setKeySplitEncryptionType(KeySplitEncryptionType.DIRECT)
            .setEncryptedKeySplit(encryptedKeySplit)
            .build();

    return executeCreateKeyRequest(createKeyRequest);
  }

  @Override
  public EncryptionKey createKey(
      EncryptionKey encryptionKey, DataKey dataKey, String encryptedKeySplit)
      throws KeyStorageServiceException {
    var apiKey = EncryptionKeyConverter.toApiEncryptionKey(encryptionKey);

    var createKeyRequest =
        CreateKeyRequest.newBuilder()
            .setKeyId(encryptionKey.getKeyId())
            .setKey(apiKey)
            .setKeySplitEncryptionType(KeySplitEncryptionType.DATA_KEY)
            .setEncryptedKeySplit(encryptedKeySplit)
            .setEncryptedKeySplitDataKey(DataKeyConverter.INSTANCE.convert(dataKey))
            .build();

    return executeCreateKeyRequest(createKeyRequest);
  }

  /**
   * Performs a getDataKey HTTP request and returns the fetched DataKey. See {@link
   * KeyStorageClient#fetchDataKey()}.
   */
  @Override
  public DataKey fetchDataKey() throws KeyStorageServiceException {
    var request = new HttpPost(getGetDataKeyUri());

    // Send an empty request.
    var payload = "{}";
    var httpResponse = executeJsonRequest(request, payload);
    var successResponse = getSuccessResponseBody(httpResponse);

    return DataKeyConverter.INSTANCE
        .reverse()
        .convert(parseSuccessGetDataKeyResponse(successResponse));
  }

  private EncryptionKey executeCreateKeyRequest(CreateKeyRequest createKeyRequest)
      throws KeyStorageServiceException {
    var request = new HttpPost(getCreateUri());

    var response = executeJsonRequest(request, serializeCreateKeyRequest(createKeyRequest));
    var responseBody = getSuccessResponseBody(response);

    return parseSuccessCreateKeyResponse(createKeyRequest.getKeyId(), responseBody);
  }

  private String serializeCreateKeyRequest(CreateKeyRequest request)
      throws KeyStorageServiceException {
    try {
      return JsonFormat.printer().print(request);
    } catch (InvalidProtocolBufferException e) {
      throw new KeyStorageServiceException("Failed to craft request", e);
    }
  }

  private HttpResponse executeJsonRequest(HttpPost request, String requestPayload)
      throws KeyStorageServiceException {
    var entity = new StringEntity(requestPayload, ContentType.APPLICATION_JSON);

    request.addHeader("content-type", "application/json");
    request.addHeader("accept", "application/json");
    request.setEntity(entity);
    request.setConfig(REQUEST_CONFIG);

    try {
      return httpClient.execute(request);
    } catch (IOException e) {
      throw new KeyStorageServiceException("HTTP Request failed", e);
    }
  }

  /**
   * Returns the string body associated with the passed in response, throwing a
   * KeyStorageServiceException if the status code is non-200 (unsuccessful).
   */
  private String getSuccessResponseBody(HttpResponse response) throws KeyStorageServiceException {
    // Note: responseEntity will return null if there's no response body.
    var responseEntity = Optional.ofNullable(response.getEntity());

    if (responseEntity.isEmpty()) {
      throw new KeyStorageServiceException("Got empty response from KeyStorageService");
    }

    String responseBody;
    try {
      responseBody = EntityUtils.toString(responseEntity.get());
    } catch (IOException e) {
      throw new KeyStorageServiceException("Failed to read responseEntity", e);
    }

    if (response.getStatusLine().getStatusCode() != HttpStatus.SC_OK) {
      var errorResponse = ErrorUtil.parseErrorResponse(responseBody);
      var exception = ErrorUtil.toServiceException(errorResponse);
      var message = "Received error from key storage service";
      throw new KeyStorageServiceException(message, exception);
    }

    return responseBody;
  }

  private DataKeyProto.DataKey parseSuccessGetDataKeyResponse(String responseBody)
      throws KeyStorageServiceException {
    try {
      DataKeyProto.DataKey.Builder builder = DataKeyProto.DataKey.newBuilder();
      JsonFormat.parser().merge(responseBody, builder);
      return builder.build();
    } catch (InvalidProtocolBufferException e) {
      throw new KeyStorageServiceException("Failed to parse success response as DataKey", e);
    }
  }

  /**
   * Attempts to read the body of a 200 response and convert it to a {@link EncryptionKey}, wrapping
   * parsing errors in a {@link KeyStorageServiceException}
   */
  private EncryptionKey parseSuccessCreateKeyResponse(String keyId, String responseBody)
      throws KeyStorageServiceException {
    try {
      EncryptionKeyProto.EncryptionKey.Builder builder =
          EncryptionKeyProto.EncryptionKey.newBuilder();
      JsonFormat.parser().merge(responseBody, builder);

      return EncryptionKeyConverter.toStorageEncryptionKey(keyId, builder.build());
    } catch (InvalidProtocolBufferException e) {
      var message = "Failed to parse success response as EncryptionKey API model";
      throw new KeyStorageServiceException(message, e);
    }
  }

  private URI getCreateUri() {
    return URI.create(String.format("%s/encryptionKeys", createKeyBaseUrl));
  }

  private URI getGetDataKeyUri() {
    return URI.create(String.format("%s/encryptionKeys:getDataKey", getDataKeyBaseUrl));
  }
}
