/*
 * Copyright 2024 Google LLC
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
package com.google.scp.operator.cpio.distributedprivacybudgetclient;

import static com.google.common.collect.ImmutableList.toImmutableList;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.coordinator.privacy.budgeting.model.ReportingOriginToPrivacyBudgetUnits;
import com.google.scp.shared.api.util.HttpClientResponse;
import com.google.scp.shared.api.util.HttpClientWithInterceptor;
import com.google.scp.shared.mapper.TimeObjectMapper;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.apache.hc.core5.http.HttpStatus;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Class responsible for making requests to a coordinator to consume budget. */
public final class PrivacyBudgetClientImplV2 implements PrivacyBudgetClientV2 {

  private static final Logger logger = LoggerFactory.getLogger(PrivacyBudgetClientImplV2.class);

  private static final String HEATH_CHECK_PATH = "/transactions:health-check";
  private static final String CONSUME_BUDGET_PATH = "/transactions:consume-budget";
  private static final String TRANSACTION_ID_HEADER_KEY = "x-gscp-transaction-id";
  private static final String TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY =
      "x-gscp-transaction-last-execution-timestamp";
  private static final String TRANSACTION_SECRET_HEADER_KEY = "x-gscp-transaction-secret";
  private static final String CLAIMED_IDENTITY_HEADER_KEY = "x-gscp-claimed-identity";

  private static final ObjectMapper mapper = new TimeObjectMapper();

  private final HttpClientWithInterceptor httpClient;
  private final String baseUrl;

  public PrivacyBudgetClientImplV2(HttpClientWithInterceptor httpClient, String baseUrl) {
    this.httpClient = httpClient;
    this.baseUrl = baseUrl;
  }

  /**
   * Performs a request against the provided baseUrl as part of the HEALTH-CHECK phase of a
   * transaction.
   */
  @Override
  public HealthCheckResponse performActionHealthCheck(TransactionRequest request) {
    HealthCheckResponse.Builder healthCheckResponse = HealthCheckResponse.builder();
    try {
      HttpClientResponse httpClientResponse =
          performTransactionPhaseAction(HEATH_CHECK_PATH, request);
      healthCheckResponse.setExecutionResult(generateExecutionResult(httpClientResponse));
    } catch (JsonProcessingException e) {
      logger.error("Serialization error.", e);
      healthCheckResponse.setExecutionResult(
          ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.MALFORMED_DATA));
    } catch (IOException e) {
      logger.error("Error performing service call.", e);
      healthCheckResponse.setExecutionResult(
          ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN));
    }
    return healthCheckResponse.build();
  }

  /**
   * Performs a request against the provided baseUrl as part of the CONSUME-BUDGET phase of a
   * transaction.
   */
  @Override
  public ConsumeBudgetResponse performActionConsumeBudget(TransactionRequest transactionRequest) {
    ConsumeBudgetResponse.Builder consumeBudgetResponse = ConsumeBudgetResponse.builder();
    try {
      HttpClientResponse httpClientResponse =
          performTransactionPhaseAction(CONSUME_BUDGET_PATH, transactionRequest);
      if (httpClientResponse.statusCode() == HttpStatus.SC_CONFLICT) {
        BudgetExhaustedResponse budgetExhaustedResponse =
            mapper.readValue(httpClientResponse.responseBody(), BudgetExhaustedResponse.class);
        ImmutableList<ReportingOriginToPrivacyBudgetUnits> requestBudgetUnitByOrigin =
            transactionRequest.reportingOriginToPrivacyBudgetUnitsList();
        ImmutableList<ReportingOriginToPrivacyBudgetUnits> exhaustedPrivacyBudgetUnitsByOrigin =
            getExhaustedPrivacyBudgetUnitsByOrigin(
                requestBudgetUnitByOrigin, budgetExhaustedResponse.budgetExhaustedIndices());
        consumeBudgetResponse
            .setExhaustedPrivacyBudgetUnitsByOrigin(exhaustedPrivacyBudgetUnitsByOrigin)
            .setExecutionResult(
                ExecutionResult.create(
                    ExecutionStatus.FAILURE, StatusCode.PRIVACY_BUDGET_CLIENT_BUDGET_EXHAUSTED));
      } else {
        consumeBudgetResponse
            .setExhaustedPrivacyBudgetUnitsByOrigin(ImmutableList.of())
            .setExecutionResult(generateExecutionResult(httpClientResponse));
      }

    } catch (JsonProcessingException e) {
      logger.error("Serialization error.", e);
      consumeBudgetResponse
          .setExhaustedPrivacyBudgetUnitsByOrigin(ImmutableList.of())
          .setExecutionResult(
              ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.MALFORMED_DATA));
    } catch (IOException e) {
      logger.error("Error performing service call.", e);
      consumeBudgetResponse
          .setExhaustedPrivacyBudgetUnitsByOrigin(ImmutableList.of())
          .setExecutionResult(ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN));
    }
    return consumeBudgetResponse.build();
  }

  /**
   * Returns the Base URL of the coordinator against which this client performs the transaction
   * actions.
   */
  @Override
  public String getPrivacyBudgetServerIdentifier() {
    return baseUrl;
  }

  private Map<String, String> getTransactionPhaseRequestHeaders(
      TransactionRequest transactionRequest, String endpointPath) {
    ImmutableMap.Builder<String, String> mapBuilder = ImmutableMap.builder();
    mapBuilder.put(
        TRANSACTION_ID_HEADER_KEY, transactionRequest.transactionId().toString().toUpperCase());
    mapBuilder.put(TRANSACTION_SECRET_HEADER_KEY, transactionRequest.transactionSecret());
    mapBuilder.put(CLAIMED_IDENTITY_HEADER_KEY, transactionRequest.claimedIdentity());

    // TODO: The server still extracts this in non-BEGIN phases. Remove both.
    if (!endpointPath.equals(HEATH_CHECK_PATH)) {
      mapBuilder.put(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, "1234");
    }

    return mapBuilder.build();
  }

  /**
   * Generates Payload for PBS ConsumeBudget request in V2 request schema.
   *
   * <pre>
   * V2 Request Schema:
   * {
   *   "v": "2.0",
   *   "data": [
   *     {
   *       "reporting_origin": <reporting_origin_string>,
   *       keys": [
   *         {
   *           "key": "<string>",
   *           "token": <uint8_t>,
   *           "reporting_time": "<string>"
   *         }
   *       ]
   *     }
   *   ]
   * }
   * </pre>
   */
  private static String generatePayload(TransactionRequest transactionRequest)
      throws JsonProcessingException {
    Map<String, Object> transactionPayload = new HashMap<>();
    transactionPayload.put("v", "2.0");
    ImmutableList<ReportingOriginToPrivacyBudgetUnits> reportingOriginToBudgetUnitsList =
        transactionRequest.reportingOriginToPrivacyBudgetUnitsList();
    List<Map<String, Object>> data = new ArrayList<>(reportingOriginToBudgetUnitsList.size());

    for (ReportingOriginToPrivacyBudgetUnits reportingOriginToBudgetUnits :
        reportingOriginToBudgetUnitsList) {
      Map<String, Object> requestDataForReportingOrigin = new HashMap<>();
      List<Map<String, Object>> keys = new ArrayList<>();
      for (PrivacyBudgetUnit budgetUnit : reportingOriginToBudgetUnits.privacyBudgetUnits()) {
        Map<String, Object> privacyBudgetKeyDetails = new HashMap<>();
        privacyBudgetKeyDetails.put("key", budgetUnit.privacyBudgetKey());
        privacyBudgetKeyDetails.put("token", transactionRequest.privacyBudgetLimit());
        privacyBudgetKeyDetails.put("reporting_time", budgetUnit.reportingWindow().toString());
        keys.add(privacyBudgetKeyDetails);
      }
      requestDataForReportingOrigin.put("keys", keys);
      requestDataForReportingOrigin.put(
          "reporting_origin", reportingOriginToBudgetUnits.reportingOrigin());
      data.add(requestDataForReportingOrigin);
    }
    transactionPayload.put("data", data);
    return mapper.writeValueAsString(transactionPayload);
  }

  private ExecutionResult generateExecutionResult(HttpClientResponse response) {
    int statusCode = response.statusCode();
    if (statusCode == HttpStatus.SC_OK) {
      return ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK);
    }

    // StatusCode 401 - The request is unauthenticated, likely due to IAM issues.
    if (statusCode == HttpStatus.SC_UNAUTHORIZED) {
      return ExecutionResult.create(
          ExecutionStatus.FAILURE, StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHENTICATED);
    }

    // StatusCode 403 - The request is authenticated, but the server refused to serve the request.
    if (statusCode == HttpStatus.SC_FORBIDDEN) {
      return ExecutionResult.create(
          ExecutionStatus.FAILURE, StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHORIZED);
    }

    if (statusCode < HttpStatus.SC_SERVER_ERROR || statusCode > 599) {
      return ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN);
    }

    return ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN);
  }

  /**
   * @brief Generates a list of budgets exhausted per origin, using origin agnostic global indices
   *     of exhausted budget units.
   *     <p>List of ReportingOriginToPrivacyBudgetUnits objects can be visualised as below
   *     <pre>
   *     [
   *       <origin1, [BudgetUnit1, BudgetUnit2, BudgetUnit5]>,
   *       <origin2, [BudgetUnit3, BudgetUnit4]>,
   *       <origin3, [BudgetUnit6, BudgetUnit8, BudgetUnit9]>
   *     ]
   *     </pre>
   *     Every row can be imagined to be one object in the list and every column to be the budget
   *     units inside that object. Every row can have a different column size. Index for BudgetUnit3
   *     in pertaining to origin2 would be 4. Similarly index for BudgetUnit4 pertaining to origin3
   *     would be 6.
   * @param requestBudgetUnitsByOrigin List of ReportingOriginToPrivacyBudgetUnits objects from
   *     which exhausted budget units are to be retrieved.
   * @param budgetExhaustedIndices List of global indices of exhausted budget units. These indices
   *     are global in the sense that, they refer to indices in a flat array created by
   *     concatenating the budget units lists from ReportingOriginToPrivacyBudgetUnits objects. Such
   *     an array for the above example would be as shown below:
   *     <pre>
   *     [BudgetUnit1, BudgetUnit2, BudgetUnit5, BudgetUnit3, BudgetUnit4, BudgetUnit6, BudgetUnit8, BudgetUnit9]
   *     </pre>
   */
  @VisibleForTesting
  ImmutableList<ReportingOriginToPrivacyBudgetUnits> getExhaustedPrivacyBudgetUnitsByOrigin(
      ImmutableList<ReportingOriginToPrivacyBudgetUnits> requestBudgetUnitsByOrigin,
      ImmutableList<Integer> budgetExhaustedIndices) {
    Set<Integer> exhaustedGlobalIndices = new HashSet<>(budgetExhaustedIndices);
    int currentIndex = 0;
    Map<String, List<PrivacyBudgetUnit>> exhaustedBudgetsByOrigin = new HashMap<>();
    for (ReportingOriginToPrivacyBudgetUnits entry : requestBudgetUnitsByOrigin) {
      String reportingOrigin = entry.reportingOrigin();
      for (PrivacyBudgetUnit unit : entry.privacyBudgetUnits()) {
        if (!exhaustedGlobalIndices.contains(currentIndex++)) {
          continue;
        }
        List<PrivacyBudgetUnit> exhaustedBudgetUnits =
            exhaustedBudgetsByOrigin.getOrDefault(reportingOrigin, new ArrayList<>());
        exhaustedBudgetUnits.add(unit);
        exhaustedBudgetsByOrigin.put(reportingOrigin, exhaustedBudgetUnits);
      }
    }
    return exhaustedBudgetsByOrigin.entrySet().stream()
        .map(
            entry ->
                ReportingOriginToPrivacyBudgetUnits.builder()
                    .setReportingOrigin(entry.getKey())
                    .setPrivacyBudgetUnits(ImmutableList.copyOf(entry.getValue()))
                    .build())
        .collect(toImmutableList());
  }

  private HttpClientResponse performTransactionPhaseAction(
      String relativePath, TransactionRequest request) throws IOException {
    final String transactionId = request.transactionId().toString();
    String payload = generatePayload(request);
    logger.info("[{}] Making POST request to {}", transactionId, baseUrl + relativePath);
    HttpClientResponse response =
        httpClient.executePost(
            baseUrl + relativePath,
            payload,
            getTransactionPhaseRequestHeaders(request, relativePath));
    logger.info("[{}] POST request response: " + response, transactionId);
    return response;
  }
}
