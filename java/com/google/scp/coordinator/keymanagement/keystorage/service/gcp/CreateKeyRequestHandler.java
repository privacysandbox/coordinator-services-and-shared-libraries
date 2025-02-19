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

package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.api.model.HttpMethod.POST;
import static com.google.scp.shared.api.util.RequestUtil.validateHttpMethod;
import static com.google.scp.shared.gcp.util.CloudFunctionUtil.createCloudFunctionResponseFromProto;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.common.collect.ImmutableMap;
import com.google.inject.Inject;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.keystorage.service.common.KeyStorageService;
import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.CreateKeyRequestProto.CreateKeyRequest;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.gcp.util.CloudFunctionRequestHandlerBase;
import java.io.IOException;

/** GCP Cloud function for handling create key requests. */
public class CreateKeyRequestHandler
    extends CloudFunctionRequestHandlerBase<CreateKeyRequest, EncryptionKey> {
  private final KeyStorageService keyStorageService;

  @Inject
  public CreateKeyRequestHandler(KeyStorageService keyStorageService) {
    this.keyStorageService = keyStorageService;
  }

  @Override
  protected CreateKeyRequest toRequest(HttpRequest httpRequest) throws ServiceException {
    validateHttpMethod(httpRequest.getMethod(), POST);
    try {
      CreateKeyRequest.Builder builder = CreateKeyRequest.newBuilder();
      JsonFormat.parser().merge(httpRequest.getReader(), builder);
      return builder.build();
    } catch (IOException e) {
      throw new ServiceException(
          INVALID_ARGUMENT, INVALID_ARGUMENT.name(), "Failed to parse CreateKeyRequest");
    }
  }

  @Override
  protected EncryptionKey processRequest(CreateKeyRequest createKeyRequest)
      throws ServiceException {
    return keyStorageService.createKey(createKeyRequest);
  }

  @Override
  protected void toCloudFunctionResponse(HttpResponse httpResponse, EncryptionKey key)
      throws IOException {
    createCloudFunctionResponseFromProto(
        httpResponse, key, OK.getHttpStatusCode(), ImmutableMap.of());
  }
}
