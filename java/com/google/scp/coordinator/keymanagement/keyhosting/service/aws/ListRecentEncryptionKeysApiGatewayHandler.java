/*
 * Copyright 2023 Google LLC
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
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.KeyService;
import com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysRequestProto.ListRecentEncryptionKeysRequest;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.ListRecentEncryptionKeysResponseProto.ListRecentEncryptionKeysResponse;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.exception.SharedErrorReason;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.aws.util.ApiGatewayHandler;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * AWS Lambda function for handling API gateway requests for fetching recently created encryption
 * keys.
 */
public class ListRecentEncryptionKeysApiGatewayHandler
    extends ApiGatewayHandler<ListRecentEncryptionKeysRequest, ListRecentEncryptionKeysResponse> {

  private static final Pattern resourcePattern = Pattern.compile("(/[^/]+)*/encryptionKeys:recent");
  private static final String MAX_AGE_SECONDS_PARAM_NAME =
      ListRecentEncryptionKeysRequest.getDescriptor()
          .findFieldByNumber(ListRecentEncryptionKeysRequest.MAX_AGE_SECONDS_FIELD_NUMBER)
          .getJsonName();

  private final KeyService keyService;

  /** Default lambda-instantiated constructor. */
  public ListRecentEncryptionKeysApiGatewayHandler() {
    this(Guice.createInjector(new AwsKeyServiceModule()).getInstance(KeyService.class));
  }

  @Inject
  public ListRecentEncryptionKeysApiGatewayHandler(KeyService keyService) {
    this.keyService = keyService;
  }

  @Override
  protected ListRecentEncryptionKeysRequest toRequest(
      APIGatewayProxyRequestEvent event, Context context) throws ServiceException {

    Matcher matcher = resourcePattern.matcher(event.getPath());
    if (!matcher.matches()) {
      throw new ServiceException(
          INVALID_ARGUMENT,
          KeyManagementErrorReason.INVALID_ARGUMENT.name(),
          String.format("Invalid URL Path, got '%s'", event.getPath()));
    }

    return ListRecentEncryptionKeysRequest.newBuilder()
        .setMaxAgeSeconds(getMaxAgeSeconds(event))
        .build();
  }

  @Override
  protected ListRecentEncryptionKeysResponse processRequest(ListRecentEncryptionKeysRequest request)
      throws ServiceException {
    return keyService.listRecentKeys(request);
  }

  @Override
  protected APIGatewayProxyResponseEvent toApiGatewayResponse(
      ListRecentEncryptionKeysResponse response) {
    return createApiGatewayResponseFromProto(response, OK.getHttpStatusCode(), allHeaders());
  }

  private int getMaxAgeSeconds(APIGatewayProxyRequestEvent event) throws ServiceException {
    if (Objects.isNull(event.getQueryStringParameters())
        || !event.getQueryStringParameters().containsKey(MAX_AGE_SECONDS_PARAM_NAME)) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          SharedErrorReason.INVALID_ARGUMENT.name(),
          String.format("%s is required.", MAX_AGE_SECONDS_PARAM_NAME));
    }
    try {
      return Integer.parseInt(event.getQueryStringParameters().get(MAX_AGE_SECONDS_PARAM_NAME));
    } catch (NumberFormatException e) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          SharedErrorReason.INVALID_ARGUMENT.name(),
          String.format("%s should be a valid integer.", MAX_AGE_SECONDS_PARAM_NAME),
          e);
    }
  }
}
