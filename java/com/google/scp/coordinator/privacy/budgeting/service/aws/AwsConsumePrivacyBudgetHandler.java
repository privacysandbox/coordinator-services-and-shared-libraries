/*
 * Copyright 2023 Google LLC
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

package com.google.scp.coordinator.privacy.budgeting.service.aws;

import static com.google.common.collect.MoreCollectors.onlyElement;
import static com.google.scp.shared.api.model.Code.INTERNAL;
import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.mapper.TimeObjectMapper.SERIALIZATION_ERROR_RESPONSE_JSON;

import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.common.collect.Streams;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Key;
import com.google.inject.Module;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;
import com.google.scp.coordinator.privacy.budgeting.service.common.Annotations.RequestAuthEnabled;
import com.google.scp.coordinator.privacy.budgeting.service.common.PrivacyBudgetingServiceModule;
import com.google.scp.coordinator.privacy.budgeting.tasks.ConsumePrivacyBudgetTask;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.exception.SharedErrorReason;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.aws.util.ApiGatewayHandler;
import com.google.scp.shared.mapper.TimeObjectMapper;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.ServiceLoader;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * AWS Lambda handler for the ConsumePrivacyBudget API. This inherently calls the {@link
 * ConsumePrivacyBudgetTask} to handle the logic for the request.
 */
public final class AwsConsumePrivacyBudgetHandler
    extends ApiGatewayHandler<ConsumePrivacyBudgetRequest, ConsumePrivacyBudgetResponse> {

  private static final Logger logger =
      LoggerFactory.getLogger(AwsConsumePrivacyBudgetHandler.class);
  private static final Injector injector = Guice.createInjector(getModule());

  private static final ConsumePrivacyBudgetTask service =
      injector.getInstance(ConsumePrivacyBudgetTask.class);
  private static final Boolean requestAuthEnabled =
      injector.getInstance(Key.get(Boolean.class, RequestAuthEnabled.class));
  private static final AwsPrivacyBudgetAccountService awsPrivacyBudgetAccountService =
      injector.getInstance(AwsPrivacyBudgetAccountService.class);

  private final ObjectMapper mapper = new TimeObjectMapper();

  /**
   * Dynamically loads all services annotated with PrivacyBudgetingServiceModule and returns the
   * only implementation of PrivacyBudgetingServiceModule that exists. This provides the flexibility
   * to provide a production and test module without exposing methods to the test class. Users
   * should only expose the production module in the production binary, and the test module in the
   * unit test. This is done by building only the production module in the production binary, and
   * the test module in the unit test.
   */
  private static Module getModule() {
    try {
      return Streams.stream(ServiceLoader.load(PrivacyBudgetingServiceModule.class))
          .collect(onlyElement());
    } catch (NoSuchElementException | IllegalArgumentException e) {
      throw new RuntimeException(e);
    }
  }

  public Injector getInjector() {
    return injector;
  }

  @Override
  protected ConsumePrivacyBudgetRequest toRequest(
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent, Context context)
      throws ServiceException {
    try {
      ConsumePrivacyBudgetRequest consumePrivacyBudgetRequest =
          mapper.readValue(
              apiGatewayProxyRequestEvent.getBody(), ConsumePrivacyBudgetRequest.class);
      logger.info(
          String.format(
              "Processing request id %s: %s",
              context.getAwsRequestId(), consumePrivacyBudgetRequest));
      return consumePrivacyBudgetRequest;
    } catch (JsonProcessingException e) {
      logger.info(
          String.format(
              "Failed to process request id %s with body %s",
              context.getAwsRequestId(), apiGatewayProxyRequestEvent.getBody()));
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          SharedErrorReason.INVALID_ARGUMENT.toString(),
          e.getOriginalMessage(),
          e);
    } catch (IllegalArgumentException e) {
      logger.info(
          String.format(
              "Failed to process request id %s with body %s",
              context.getAwsRequestId(), apiGatewayProxyRequestEvent.getBody()));
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          SharedErrorReason.INVALID_ARGUMENT.toString(),
          String.format(
              "Unable to parse request body. Request body was: %s",
              apiGatewayProxyRequestEvent.getBody()),
          e);
    }
  }

  @Override
  protected void authorizeRequest(
      ConsumePrivacyBudgetRequest consumePrivacyBudgetRequest,
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent)
      throws ServiceException {
    if (requestAuthEnabled) {
      awsPrivacyBudgetAccountService.authorizeRequest(
          consumePrivacyBudgetRequest, apiGatewayProxyRequestEvent);
    }
  }

  @Override
  protected ConsumePrivacyBudgetResponse processRequest(
      ConsumePrivacyBudgetRequest consumePrivacyBudgetRequest) throws ServiceException {

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        service.handleRequest(consumePrivacyBudgetRequest);
    logger.info(
        String.format(
            "Processed request: %s. The response is: %s",
            consumePrivacyBudgetRequest, consumePrivacyBudgetResponse));
    return consumePrivacyBudgetResponse;
  }

  @Override
  protected APIGatewayProxyResponseEvent toApiGatewayResponse(
      ConsumePrivacyBudgetResponse response) {
    return createApiGatewayResponse(
        response, /* httpStatusCode = */ OK.getHttpStatusCode(), allHeaders());
  }

  /**
   * Sets body, statusCode, and headers for returned APIGatewayProxyResponseEvent. If there is a
   * JsonProcessingException, returns generic error and logs internal details
   */
  private APIGatewayProxyResponseEvent createApiGatewayResponse(
      ConsumePrivacyBudgetResponse responseBody, int code, Map<String, String> headers) {
    APIGatewayProxyResponseEvent response = new APIGatewayProxyResponseEvent();
    try {
      response.setBody(mapper.writeValueAsString(responseBody));
      response.setStatusCode(code);
      if (!headers.isEmpty()) {
        response.setHeaders(headers);
      }
    } catch (JsonProcessingException exception) {
      logger.error("Serialization error", exception);
      response.setBody(SERIALIZATION_ERROR_RESPONSE_JSON);
      response.setStatusCode(INTERNAL.getHttpStatusCode());
    }
    return response;
  }
}
