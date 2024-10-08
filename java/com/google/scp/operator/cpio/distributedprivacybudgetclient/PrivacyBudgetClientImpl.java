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

package com.google.scp.operator.cpio.distributedprivacybudgetclient;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionPhase.BEGIN;
import static com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionPhase.COMMIT;
import static com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionPhase.END;
import static com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionPhase.NOTIFY;
import static com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionPhase.PREPARE;
import static com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionPhase.UNKNOWN;

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

/**
 * This class responsible for making API requests to a given Pbs coordinator based on the
 * transaction details
 */
public final class PrivacyBudgetClientImpl implements PrivacyBudgetClient {

  private static final Logger logger = LoggerFactory.getLogger(PrivacyBudgetClientImpl.class);

  private static final String BEGIN_TRANSACTION_PATH = "/transactions:begin";
  private static final String PREPARE_TRANSACTION_PATH = "/transactions:prepare";
  private static final String COMMIT_TRANSACTION_PATH = "/transactions:commit";
  private static final String NOTIFY_TRANSACTION_PATH = "/transactions:notify";
  private static final String ABORT_TRANSACTION_PATH = "/transactions:abort";
  private static final String END_TRANSACTION_PATH = "/transactions:end";
  private static final String TRANSACTION_STATUS_PATH = "/transactions:status";
  private static final String TRANSACTION_ID_HEADER_KEY = "x-gscp-transaction-id";
  private static final String TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY =
      "x-gscp-transaction-last-execution-timestamp";
  private static final String TRANSACTION_SECRET_HEADER_KEY = "x-gscp-transaction-secret";
  private static final String CLAIMED_IDENTITY_HEADER_KEY = "x-gscp-claimed-identity";
  private static final String CLIENT_VERSION_HEADER_KEY = "x-trusted-services-client-version";

  private static final List<Integer> HTTP_CODES_FOR_STATUS_CHECK_INVOCATION =
      ImmutableList.of(HttpStatus.SC_CLIENT_ERROR, HttpStatus.SC_PRECONDITION_FAILED);

  private static final ObjectMapper mapper = new TimeObjectMapper();

  private final HttpClientWithInterceptor httpClient;
  private final String baseUrl;

  public PrivacyBudgetClientImpl(HttpClientWithInterceptor httpClient, String baseUrl) {
    this.httpClient = httpClient;
    this.baseUrl = baseUrl;
  }

  /**
   * Performs request against the provided baseUrl as part of BEGIN phase of transaction. Updates
   * the transaction's lastExecutionTimestamp field in case of successful operation
   *
   * @param transaction -The transaction which the action pertains to
   * @return ExecutionResult The execution result of the operation.
   */
  @Override
  public ExecutionResult performActionBegin(Transaction transaction)
      throws PrivacyBudgetClientException {
    return performTransactionPhaseAction(BEGIN_TRANSACTION_PATH, transaction);
  }

  /**
   * Performs request against the provided baseUrl as part of PREPARE phase of transaction. Updates
   * the transaction's lastExecutionTimestamp field in case of successful operation
   *
   * @param transaction -The transaction which the action pertains to
   * @return ExecutionResult The execution result of the operation.
   */
  @Override
  public ExecutionResult performActionPrepare(Transaction transaction)
      throws PrivacyBudgetClientException {
    return performTransactionPhaseAction(PREPARE_TRANSACTION_PATH, transaction);
  }

  /**
   * Performs request against the provided baseUrl as part of COMMIT phase of transaction. Updates
   * the transaction's lastExecutionTimestamp field in case of successful operation
   *
   * @param transaction -The transaction which the action pertains to
   * @return ExecutionResult The execution result of the operation.
   */
  @Override
  public ExecutionResult performActionCommit(Transaction transaction)
      throws PrivacyBudgetClientException {
    return performTransactionPhaseAction(COMMIT_TRANSACTION_PATH, transaction);
  }

  /**
   * Performs request against the provided baseUrl as part of NOTIFY phase of transaction. Updates
   * the transaction's lastExecutionTimestamp field in case of successful operation
   *
   * @param transaction -The transaction which the action pertains to
   * @return ExecutionResult The execution result of the operation.
   */
  @Override
  public ExecutionResult performActionNotify(Transaction transaction)
      throws PrivacyBudgetClientException {
    return performTransactionPhaseAction(NOTIFY_TRANSACTION_PATH, transaction);
  }

  /**
   * Performs request against the provided baseUrl as part of END phase of transaction. Updates the
   * transaction's lastExecutionTimestamp field in case of successful operation
   *
   * @param transaction -The transaction which the action pertains to
   * @return ExecutionResult The execution result of the operation.
   */
  @Override
  public ExecutionResult performActionEnd(Transaction transaction)
      throws PrivacyBudgetClientException {
    return performTransactionPhaseAction(END_TRANSACTION_PATH, transaction);
  }

  /**
   * Performs request against the provided baseUrl as part of ABORT phase of transaction. Updates
   * the transaction's lastExecutionTimestamp field in case of successful operation
   *
   * @param transaction -The transaction which the action pertains to
   * @return ExecutionResult The execution result of the operation.
   */
  @Override
  public ExecutionResult performActionAbort(Transaction transaction)
      throws PrivacyBudgetClientException {
    return performTransactionPhaseAction(ABORT_TRANSACTION_PATH, transaction);
  }

  @Override
  public String getPrivacyBudgetServerIdentifier() {
    return baseUrl;
  }

  private void updateTransactionState(Transaction transaction, HttpClientResponse response) {
    if (response.statusCode() == 200) {
      String lastExecTimestamp = response.headers().get(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY);
      transaction.setLastExecutionTimestamp(baseUrl, lastExecTimestamp);
    }
  }

  private TransactionStatusResponse fetchTransactionStatus(Transaction transaction)
      throws IOException {
    final String transactionId = transaction.getId().toString();
    ImmutableMap<String, String> headers =
        new ImmutableMap.Builder<String, String>()
            .put(TRANSACTION_ID_HEADER_KEY, transaction.getId().toString().toUpperCase())
            .put(TRANSACTION_SECRET_HEADER_KEY, transaction.getRequest().transactionSecret())
            .put(CLAIMED_IDENTITY_HEADER_KEY, transaction.getRequest().claimedIdentity())
            .put(CLIENT_VERSION_HEADER_KEY, transaction.getRequest().trustedServicesClientVersion())
            .build();
    logger.info(
        "[{}] Making GET request to {}",
        transaction.getId().toString(),
        baseUrl + TRANSACTION_STATUS_PATH);
    var response = httpClient.executeGet(baseUrl + TRANSACTION_STATUS_PATH, headers);
    logger.info("[{}] GET request response: " + response, transactionId);
    return generateTransactionStatus(response);
  }

  private Map<String, String> getTransactionPhaseRequestHeaders(Transaction transaction) {
    ImmutableMap.Builder<String, String> mapBuilder = ImmutableMap.builder();
    mapBuilder.put(TRANSACTION_ID_HEADER_KEY, transaction.getId().toString().toUpperCase());
    mapBuilder.put(TRANSACTION_SECRET_HEADER_KEY, transaction.getRequest().transactionSecret());
    mapBuilder.put(CLAIMED_IDENTITY_HEADER_KEY, transaction.getRequest().claimedIdentity());
    mapBuilder.put(
        CLIENT_VERSION_HEADER_KEY, transaction.getRequest().trustedServicesClientVersion());
    if (!transaction.getCurrentPhase().equals(BEGIN)) {
      String lastExecTimestamp = transaction.getLastExecutionTimestamp(baseUrl);
      mapBuilder.put(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, lastExecTimestamp);
    }
    return mapBuilder.build();
  }

  /**
   * Generates Payload for PBS ConsumeBudget request in V2 request schema
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
  private String generatePayload(Transaction transaction) throws JsonProcessingException {
    Map<String, Object> transactionPayload = new HashMap<>();
    transactionPayload.put("v", "2.0");
    TransactionRequest transactionRequest = transaction.getRequest();
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

  private ExecutionResult generateExecutionResult(
      Transaction transaction, HttpClientResponse response) throws PrivacyBudgetClientException {
    int statusCode = response.statusCode();
    if (statusCode == HttpStatus.SC_OK) {
      return ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK);
    }
    if (HTTP_CODES_FOR_STATUS_CHECK_INVOCATION.contains(statusCode)) {
      // StatusCode 412 - Returned when last execution timestamp sent by the client does not match
      // what server has
      // StatusCode 400 - One of the reasons it can be returned is if client is trying to execute a
      // phase that is different from what server is expecting
      return getExecutionResultBasedOnTransactionStatus(transaction, statusCode);
    }
    if (statusCode == HttpStatus.SC_CONFLICT) {
      // StatusCode 409 is returned when certain keys have their budgets exhausted.
      try {
        processBudgetExhaustedResponse(response, transaction);
      } catch (JsonProcessingException e) {
        throw new PrivacyBudgetClientException(e.getMessage());
      }
      return ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN);
    }
    // StatusCode 401 - The request is unauthenticated, likely due to IAM issues.
    if (statusCode == HttpStatus.SC_UNAUTHORIZED) {
      return ExecutionResult.create(
          ExecutionStatus.FAILURE, StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHENTICATED);
    }
    // StatusCode 403 - The request is authenticated, but server refuse to serve the request.
    if (statusCode == HttpStatus.SC_FORBIDDEN) {
      return ExecutionResult.create(
          ExecutionStatus.FAILURE, StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHORIZED);
    }
    if (statusCode < HttpStatus.SC_SERVER_ERROR || statusCode > 599) {
      return ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN);
    }
    return ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN);
  }

  private void processBudgetExhaustedResponse(HttpClientResponse response, Transaction transaction)
      throws JsonProcessingException {
    BudgetExhaustedResponse budgetExhaustedResponse =
        mapper.readValue(response.responseBody(), BudgetExhaustedResponse.class);
    ImmutableList<ReportingOriginToPrivacyBudgetUnits> requestBudgetUnitByOrigin =
        transaction.getRequest().reportingOriginToPrivacyBudgetUnitsList();
    ImmutableList<ReportingOriginToPrivacyBudgetUnits> exhaustedPrivacyBudgetUnitsByOrigin =
        getExhaustedPrivacyBudgetUnitsByOrigin(
            requestBudgetUnitByOrigin, budgetExhaustedResponse.budgetExhaustedIndices());

    transaction.setExhaustedPrivacyBudgetUnits(exhaustedPrivacyBudgetUnitsByOrigin);
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

  /**
   * Returns a status of {@link ExecutionStatus.FAILURE} if the transaction's status is unable to be
   * fetched.
   *
   * <p>Returns a status of {@link ExecutionStatus.RETRY} if either:
   *
   * <ul>
   *   <li>A status of transaction phase failure is received
   *   <li>the transaction phase returned matches the current transaction phase. Updates
   *       lastExecutionTimestamp based on received status.
   * </ul>
   *
   * <p>Returns a status of {@link Execution.SUCCESS} if both:
   *
   * <ul>
   *   <li>The status received is not "failure"
   *   <li>The transaction phase returned does not match the current transaction phase. Updates
   *       lastExecutionTimestamp based on received status
   * </ul>
   *
   * @param transaction - Transaction whose status needs to be checked against the given coordinator
   * @param actionHttpStatusCode - The Http status code returned by the transaction phase request
   * @throws PrivacyBudgetClientException - in case the status received is transaction expired or
   *     the transaction phase between client and server are out sync by more than 1 phase
   */
  private ExecutionResult getExecutionResultBasedOnTransactionStatus(
      Transaction transaction, int actionHttpStatusCode) throws PrivacyBudgetClientException {
    if (!HTTP_CODES_FOR_STATUS_CHECK_INVOCATION.contains((actionHttpStatusCode))) {
      // HTTP code is one of the codes which is not expected to be resolved using status API
      return ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN);
    }
    TransactionStatusResponse transactionStatusResponse;
    try {
      transactionStatusResponse = fetchTransactionStatus(transaction);
    } catch (Exception e) {
      logger.error(
          "[{}] Failed to fetch transaction status. Error is: {}",
          transaction.getId(),
          e.getMessage());
      return ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN);
    }
    if (transactionStatusResponse.isExpired()) {
      throw new PrivacyBudgetClientException(
          StatusCode.TRANSACTION_ENGINE_TRANSACTION_IS_EXPIRED.name());
    }
    transaction.setLastExecutionTimestamp(
        baseUrl, transactionStatusResponse.lastExecutionTimestamp());
    if (transactionStatusResponse.hasFailed()) {
      return ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN);
    }
    TransactionPhase serverPhase = transactionStatusResponse.currentPhase();
    TransactionPhase clientPhase = transaction.getCurrentPhase();
    TransactionPhase clientNextPhaseOnSuccess = getNextPhaseOnSuccess(clientPhase);
    if (serverPhase == clientNextPhaseOnSuccess) {
      // The phase has succeeded on the server. Client can mark this phase as success as well and
      // proceed.
      return ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK);
    } else if (serverPhase == clientPhase) {
      if (actionHttpStatusCode == HttpStatus.SC_CLIENT_ERROR) {
        // The cause of the 400 is not due to phase differences.
        return ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN);
      } else if (actionHttpStatusCode == HttpStatus.SC_PRECONDITION_FAILED) {
        // The cause of the 412 is not due to phase differences but due to timestamp
        // difference. Retry will fix it
        return ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN);
      } else {
        // Should not reach here.
        // HTTP code is one of the codes which is not expected to be resolved using status API
        return ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN);
      }
    } else {
      // Server phase is either behind client or much ahead of client phase.
      throw new PrivacyBudgetClientException(
          String.format(
              "The PrivacyBudget client and server phases are out of sync. server phase"
                  + " value: %s. client phase value: %s. Transaction cannot be"
                  + " completed",
              serverPhase, clientPhase));
    }
  }

  private static TransactionPhase getNextPhaseOnSuccess(TransactionPhase currentPhase) {
    switch (currentPhase) {
      case NOTSTARTED:
        return BEGIN;
      case BEGIN:
        return PREPARE;
      case PREPARE:
        return COMMIT;
      case COMMIT:
        return NOTIFY;
      case NOTIFY:
      case ABORT:
        return END;
      default:
        return UNKNOWN;
    }
  }

  private TransactionStatusResponse generateTransactionStatus(HttpClientResponse response)
      throws IOException {
    int statusCode = response.statusCode();
    if (statusCode == 200) {
      return mapper.readValue(response.responseBody(), TransactionStatusResponse.class);
    } else {
      throw new IOException(
          String.format("Error retrieving transaction status. Error details: %s", response));
    }
  }

  private ExecutionResult performTransactionPhaseAction(
      String relativePath, Transaction transaction) throws PrivacyBudgetClientException {
    try {
      final String transactionId = transaction.getId().toString();
      String payload = generatePayload(transaction);
      logger.info("[{}] Making POST request to {}", transactionId, baseUrl + relativePath);
      var response =
          httpClient.executePost(
              baseUrl + relativePath, payload, getTransactionPhaseRequestHeaders(transaction));
      logger.info("[{}] POST request response: " + response, transactionId);
      updateTransactionState(transaction, response);
      return generateExecutionResult(transaction, response);
    } catch (JsonProcessingException e) {
      logger.error("Serialization error", e);
      return ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.MALFORMED_DATA);
    } catch (IOException e) {
      logger.error("Error performing service call", e);
      return ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN);
    }
  }
}
