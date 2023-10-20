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

package com.google.scp.operator.cpio.privacybudgetclient;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.common.primitives.Ints;
import com.google.inject.Inject;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;
import com.google.scp.operator.cpio.privacybudgetclient.HttpPrivacyBudgetClientModule.PrivacyBudgetServiceBaseUrl;
import com.google.scp.shared.api.util.ErrorUtil;
import com.google.scp.shared.mapper.TimeObjectMapper;
import java.io.IOException;
import java.net.URI;
import java.time.Duration;
import org.apache.http.HttpStatus;
import org.apache.http.client.HttpClient;
import org.apache.http.client.config.RequestConfig;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.ContentType;
import org.apache.http.entity.StringEntity;
import org.apache.http.util.EntityUtils;

/** Client which interacts with privacy budget service. */
public final class HttpPrivacyBudgetClient implements PrivacyBudgetClient {

  private static final int REQUEST_TIMEOUT_DURATION =
      Ints.checkedCast(Duration.ofMinutes(1).toMillis());
  private static final int PRIVACY_BUDGET_MAX_ITEM = 30000;
  // Base URL (e.g. `https://foo.com/v1`).
  private final String privacyBudgetServiceBaseUrl;
  private final HttpClient httpClient;
  private final ObjectMapper mapper = new TimeObjectMapper();
  private static final RequestConfig requestConfig =
      RequestConfig.custom()
          // Timeout for requesting a connection from internal connection manager
          .setConnectionRequestTimeout(REQUEST_TIMEOUT_DURATION)
          // Timeout for establishing a request to host
          .setConnectTimeout(REQUEST_TIMEOUT_DURATION)
          // Timeout between data packets received
          .setSocketTimeout(REQUEST_TIMEOUT_DURATION)
          .build();

  /** Creates a new instance of the {@code HttpPrivacyBudgetClient} class. */
  @Inject
  public HttpPrivacyBudgetClient(
      @PrivacyBudgetServiceBaseUrl String privacyBudgetServiceBaseUrl, HttpClient httpClient) {
    this.privacyBudgetServiceBaseUrl = privacyBudgetServiceBaseUrl;
    this.httpClient = httpClient;
  }

  @Override
  public ConsumePrivacyBudgetResponse consumePrivacyBudget(
      ConsumePrivacyBudgetRequest privacyBudget)
      throws PrivacyBudgetServiceException, PrivacyBudgetClientException {
    var size = privacyBudget.privacyBudgetUnits().size();
    if (size > PRIVACY_BUDGET_MAX_ITEM) {
      throw new PrivacyBudgetClientException(
          String.format("Too many items. Current: %d; Max: %d", size, PRIVACY_BUDGET_MAX_ITEM));
    }
    try {
      var request = new HttpPost(URI.create(privacyBudgetServiceBaseUrl));
      request.setConfig(requestConfig);

      StringEntity entity =
          new StringEntity(mapper.writeValueAsString(privacyBudget), ContentType.APPLICATION_JSON);
      request.addHeader("content-type", "application/json");
      request.setEntity(entity);

      var response = httpClient.execute(request);
      var responseBody = EntityUtils.toString(response.getEntity());

      if (response.getStatusLine().getStatusCode() != HttpStatus.SC_OK) {
        var errorResponse = ErrorUtil.parseErrorResponse(responseBody);
        var exception = ErrorUtil.toServiceException(errorResponse);

        var message = "Received error from privacy budget service";
        throw new PrivacyBudgetServiceException(message, exception);
      }
      return parseSuccessResponse(responseBody);
    } catch (IOException | IllegalArgumentException e) {
      var message = "Failed consuming the privacy budget";
      throw new PrivacyBudgetServiceException(message, e);
    }
  }

  /**
   * Attempts to read the body of a 200 response and convert it to a {@link
   * ConsumePrivacyBudgetResponse}, wrapping parsing errors in a {@link
   * PrivacyBudgetServiceException}.
   */
  private ConsumePrivacyBudgetResponse parseSuccessResponse(String responseBody)
      throws PrivacyBudgetServiceException {
    try {
      return mapper.readValue(responseBody, ConsumePrivacyBudgetResponse.class);
    } catch (JsonProcessingException e) {
      var message = "Failed to parse success response as ConsumePrivacyBudgetResponse";
      throw new PrivacyBudgetServiceException(message, e);
    }
  }
}
