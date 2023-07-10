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

package com.google.scp.coordinator.keymanagement.keyhosting.service.aws;

import static com.google.scp.coordinator.keymanagement.keyhosting.service.common.KeyHostingUtil.CACHE_CONTROL_HEADER_NAME;
import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.aws.util.LambdaHandlerUtil.createApiGatewayResponseFromProto;

import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.common.collect.ImmutableMap;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.GetActivePublicKeysResponseWithHeaders;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.KeyService;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetActivePublicKeysRequestProto.GetActivePublicKeysRequest;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.aws.util.ApiGatewayHandler;

/**
 * Handles requests to PublicKey API from API Gateway. Always returns APIGatewayProxyResponseEvent.
 * Body is list of public keys if successful or an ErrorResponse with error info.
 */
public class PublicKeyApiGatewayHandler
    extends ApiGatewayHandler<GetActivePublicKeysRequest, GetActivePublicKeysResponseWithHeaders> {

  private final KeyService keyService;

  /** Default no-args constructor is for Lambda instantiation */
  public PublicKeyApiGatewayHandler() {
    Injector injector = Guice.createInjector(new AwsKeyServiceModule());
    this.keyService = injector.getInstance(KeyService.class);
  }

  /** Constructor for Guice instantiation in tests */
  public PublicKeyApiGatewayHandler(KeyService keyService) {
    this.keyService = keyService;
  }

  @Override
  protected GetActivePublicKeysRequest toRequest(
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent, Context context)
      throws ServiceException {
    return GetActivePublicKeysRequest.newBuilder().build();
  }

  @Override
  protected GetActivePublicKeysResponseWithHeaders processRequest(
      GetActivePublicKeysRequest getActivePublicKeysRequest) throws ServiceException {

    return keyService.getActivePublicKeys();
  }

  @Override
  protected APIGatewayProxyResponseEvent toApiGatewayResponse(
      GetActivePublicKeysResponseWithHeaders response) {
    ImmutableMap.Builder<String, String> headers = ImmutableMap.builder();
    if (response.cacheControlMaxAge().isPresent()) {
      headers.put(CACHE_CONTROL_HEADER_NAME, response.cacheControlMaxAge().get());
    }
    headers.putAll(allHeaders());

    return createApiGatewayResponseFromProto(
        /* bodyObject = */ response.getActivePublicKeysResponse(),
        /* httpStatusCode = */ OK.getHttpStatusCode(),
        /* headers = */ headers.build());
  }
}
