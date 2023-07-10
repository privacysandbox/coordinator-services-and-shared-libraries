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

package com.google.scp.coordinator.keymanagement.keystorage.service.aws;

import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.aws.util.LambdaHandlerUtil.createApiGatewayResponseFromProto;

import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.keystorage.service.common.KeyStorageService;
import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.CreateKeyRequestProto.CreateKeyRequest;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.aws.util.ApiGatewayHandler;
import java.util.Optional;

/** AWS Lambda function for handling key storage requests. */
public class CreateKeyApiGatewayHandler extends ApiGatewayHandler<CreateKeyRequest, EncryptionKey> {

  private final KeyStorageService keyStorageService;

  /** Default lambda-instantiated constructor. */
  public CreateKeyApiGatewayHandler() {
    Injector injector = Guice.createInjector(new AwsKeyStorageServiceModule());
    this.keyStorageService = injector.getInstance(KeyStorageService.class);
  }

  /** Test-only constructor. */
  public CreateKeyApiGatewayHandler(KeyStorageService keyStorageService) {
    this.keyStorageService = keyStorageService;
  }

  /** Converts an API gateway request to the internal Java request format. */
  @Override
  protected CreateKeyRequest toRequest(APIGatewayProxyRequestEvent request, Context ctx)
      throws ServiceException {
    try {
      var requestBody = Optional.ofNullable(request.getBody());
      if (requestBody.isEmpty()) {
        throw new ServiceException(
            INVALID_ARGUMENT, INVALID_ARGUMENT.name(), "Missing request body");
      }
      CreateKeyRequest.Builder builder = CreateKeyRequest.newBuilder();
      JsonFormat.parser().merge(requestBody.get(), builder);
      return builder.build();
    } catch (InvalidProtocolBufferException e) {
      throw new ServiceException(
          INVALID_ARGUMENT, INVALID_ARGUMENT.name(), "Failed to parse CreateKeyRequest");
    }
  }

  @Override
  protected EncryptionKey processRequest(CreateKeyRequest request) throws ServiceException {
    return keyStorageService.createKey(request);
  }

  @Override
  protected APIGatewayProxyResponseEvent toApiGatewayResponse(EncryptionKey key) {
    return createApiGatewayResponseFromProto(key, OK.getHttpStatusCode(), allHeaders());
  }
}
