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

import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.aws.util.LambdaHandlerUtil.createApiGatewayResponseFromProto;

import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.KeyService;
import com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetEncryptedPrivateKeyRequestProto.GetEncryptedPrivateKeyRequest;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetEncryptedPrivateKeyResponseProto.GetEncryptedPrivateKeyResponse;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.aws.util.ApiGatewayHandler;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** AWS Lambda function for handling requests for fetching a specific encrypted private key. */
public class PrivateKeyApiGatewayHandler
    extends ApiGatewayHandler<GetEncryptedPrivateKeyRequest, GetEncryptedPrivateKeyResponse> {

  // Example: /v1/privateKeys/abc-123 or /privateKeys/abc-123
  private static final Pattern resourcePattern = Pattern.compile("/?(?<name>privateKeys/[^/]+)");

  private final KeyService keyService;

  /** Default lambda-instantiated constructor. */
  public PrivateKeyApiGatewayHandler() {
    Injector injector = Guice.createInjector(new AwsKeyServiceModule());
    this.keyService = injector.getInstance(KeyService.class);
  }

  /** Test-only constructor. */
  public PrivateKeyApiGatewayHandler(KeyService keyService) {
    this.keyService = keyService;
  }

  @Override
  protected GetEncryptedPrivateKeyRequest toRequest(
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent, Context context)
      throws ServiceException {
    Matcher m = resourcePattern.matcher(apiGatewayProxyRequestEvent.getPath());
    if (!m.find()) {
      throw new ServiceException(
          INVALID_ARGUMENT,
          KeyManagementErrorReason.INVALID_ARGUMENT.name(),
          String.format("Invalid URL Path, got '%s'", apiGatewayProxyRequestEvent.getPath()));
    }
    return GetEncryptedPrivateKeyRequest.newBuilder().setName(m.group("name")).build();
  }

  @Override
  protected GetEncryptedPrivateKeyResponse processRequest(
      GetEncryptedPrivateKeyRequest getEncryptedPrivateKeyRequest) throws ServiceException {
    return keyService.getEncryptedPrivateKey(getEncryptedPrivateKeyRequest);
  }

  @Override
  protected APIGatewayProxyResponseEvent toApiGatewayResponse(
      GetEncryptedPrivateKeyResponse response) {
    return createApiGatewayResponseFromProto(
        /* bodyObject = */ response,
        /* httpStatusCode = */ OK.getHttpStatusCode(),
        /* headers = */ allHeaders());
  }
}
