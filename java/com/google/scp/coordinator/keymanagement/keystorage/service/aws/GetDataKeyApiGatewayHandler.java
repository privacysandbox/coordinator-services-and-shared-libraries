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

import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.aws.util.LambdaHandlerUtil.createApiGatewayResponseFromProto;

import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.scp.coordinator.keymanagement.keystorage.service.common.KeyStorageService;
import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.DataKeyProto.DataKey;
import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.GetDataKeyRequestProto.GetDataKeyRequest;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.aws.util.ApiGatewayHandler;

/**
 * AWS Lambda function for generating DataKeys used for encrypting keysplits sent to
 * KeyStorageService.
 */
public class GetDataKeyApiGatewayHandler extends ApiGatewayHandler<GetDataKeyRequest, DataKey> {

  private final KeyStorageService keyStorageService;

  /** Default lambda-instantiated constructor. */
  public GetDataKeyApiGatewayHandler() {
    Injector injector = Guice.createInjector(new AwsKeyStorageServiceModule());
    this.keyStorageService = injector.getInstance(KeyStorageService.class);
  }

  /** Test-only constructor. */
  public GetDataKeyApiGatewayHandler(KeyStorageService keyStorageService) {
    this.keyStorageService = keyStorageService;
  }

  @Override
  protected GetDataKeyRequest toRequest(APIGatewayProxyRequestEvent request, Context ctx)
      throws ServiceException {
    // No arguments.
    return GetDataKeyRequest.newBuilder().build();
  }

  @Override
  protected DataKey processRequest(GetDataKeyRequest request) throws ServiceException {
    return keyStorageService.getDataKey();
  }

  @Override
  protected APIGatewayProxyResponseEvent toApiGatewayResponse(DataKey response) {
    return createApiGatewayResponseFromProto(response, OK.getHttpStatusCode(), allHeaders());
  }
}
