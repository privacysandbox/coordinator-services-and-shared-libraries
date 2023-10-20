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

package com.google.scp.shared.aws.gateway.events;

import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyRequestEvent;
import java.util.Map;

/**
 * GetApiGatewayEvent is inherited for Api Gateway handlers that expose GET Rest endpoints.
 *
 * @param <TResponse> The data type of the response object that this endpoint returns.
 */
public abstract class GetApiGatewayEvent<TResponse> extends BaseApiGatewayEvent<TResponse> {

  @Override
  protected TResponse handleRequestEvent(
      APIGatewayProxyRequestEvent apiGatewayProxyRequestEvent, Context context) throws Exception {
    Map<String, String> queryParams = apiGatewayProxyRequestEvent.getQueryStringParameters();
    return handleGet(queryParams);
  }

  /**
   * Inherited classes must override this function to receive a Map of the query string from the GET
   * request.
   */
  protected abstract TResponse handleGet(Map<String, String> queryParams) throws Exception;
}
