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

package com.google.scp.shared.aws.gateway;

import static com.google.common.truth.Truth.assertThat;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.scp.shared.mapper.TimeObjectMapper;

/** Contains common assertions used for testing APIGatewayProxyResponseEvents. */
public final class ResponseEventAssertions {
  private static final ObjectMapper objectMapper = new TimeObjectMapper();

  /**
   * Asserts that the content body of an APIGatewayProxyResponseEvent contains the json serialized
   * string of a given input.
   */
  public static <TBody> void assertThatResponseBodyContains(
      APIGatewayProxyResponseEvent responseEvent, TBody equalTo) throws JsonProcessingException {
    assertThat(responseEvent.getBody()).isEqualTo(objectMapper.writeValueAsString(equalTo));
  }

  /**
   * Asserts that the content body of an APIGatewayProxyResponseEvent contains the json serialized
   * string of a given input.
   */
  public static <TBody> void assertThatResponseBodyContains(
      APIGatewayProxyResponseEvent responseEvent, TBody equalTo, Class<TBody> type)
      throws JsonProcessingException {
    assertThat(objectMapper.readValue(responseEvent.getBody(), type)).isEqualTo(equalTo);
  }

  /**
   * Asserts that the content body of an APIGatewayProxyResponseEvent does not contain the json
   * serialized string of a given input.
   */
  public static <TBody> void assertThatResponseBodyDoesNotContain(
      APIGatewayProxyResponseEvent responseEvent, TBody equalTo) throws JsonProcessingException {
    assertThat(responseEvent.getBody()).isNotEqualTo(objectMapper.writeValueAsString(equalTo));
  }
}
