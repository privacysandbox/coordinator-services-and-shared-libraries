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

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.PrivacyBudgetClient.PrivacyBudgetClientException;
import com.google.scp.shared.api.util.HttpClientResponse;
import com.google.scp.shared.api.util.HttpClientWithInterceptor;
import com.google.scp.shared.mapper.TimeObjectMapper;
import java.io.IOException;
import java.sql.Timestamp;
import java.time.Instant;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

public final class PrivacyBudgetClientTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private HttpClientWithInterceptor awsHttpClient;

  private static final ObjectMapper mapper = new TimeObjectMapper();

  private static final UUID TRANSACTION_ID = UUID.randomUUID();
  private static final String ENDPOINT = "http://www.google.com/v1";

  private static final String BEGIN_PHASE_URI = ENDPOINT + "/transactions:begin";
  private static final String PREPARE_PHASE_URI = ENDPOINT + "/transactions:prepare";
  private static final String NOTIFY_PHASE_URI = ENDPOINT + "/transactions:notify";
  private static final String COMMIT_PHASE_URI = ENDPOINT + "/transactions:commit";
  private static final String END_PHASE_URI = ENDPOINT + "/transactions:end";
  private static final String ABORT_PHASE_URI = ENDPOINT + "/transactions:abort";
  private static final String TRANSACTION_STATUS_URI = ENDPOINT + "/transactions:status";

  private static final String TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY =
      "x-gscp-transaction-last-execution-timestamp";
  private TransactionRequest transactionRequest;
  private HttpClientResponse successResponse;
  private HttpClientResponse retryableFailureResponse;
  private HttpClientResponse nonRetryableFailureResponse;
  private HttpClientResponse preconditionNotMetFailureResponse;
  private HttpClientResponse badRequestFailureResponse;
  private HttpClientResponse budgetExhaustedResponse;
  private HttpClientResponse unauthenticatedResponse;
  private HttpClientResponse unauthorizedResponse;

  private PrivacyBudgetClientImpl privacyBudgetClient;

  @Before
  public void setup() {
    this.privacyBudgetClient = new PrivacyBudgetClientImpl(awsHttpClient, ENDPOINT);
    transactionRequest = generateTransactionRequest();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY,
            String.valueOf(Instant.now().toEpochMilli()));
    successResponse = HttpClientResponse.create(200, "", responseHeaders);
    retryableFailureResponse = HttpClientResponse.create(500, "", Collections.emptyMap());
    nonRetryableFailureResponse = HttpClientResponse.create(400, "", Collections.emptyMap());
    preconditionNotMetFailureResponse = HttpClientResponse.create(412, "", Collections.emptyMap());
    badRequestFailureResponse = HttpClientResponse.create(400, "", Collections.emptyMap());
    budgetExhaustedResponse =
        HttpClientResponse.create(409, "{\"f\":[0],\"v\":\"1.0\"}", Collections.emptyMap());
    unauthenticatedResponse = HttpClientResponse.create(401, "", Collections.emptyMap());
    unauthorizedResponse = HttpClientResponse.create(403, "", Collections.emptyMap());
  }

  @Test
  public void performAction_begin_success() throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.BEGIN, transactionRequest);
    when(awsHttpClient.executePost(
            eq(BEGIN_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(successResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionBegin(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performAction_begin_retriableFailure()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.BEGIN, transactionRequest);
    when(awsHttpClient.executePost(
            eq(BEGIN_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(retryableFailureResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionBegin(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.RETRY);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  @Test
  public void performAction_begin_nonRetriableFailure()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.BEGIN, transactionRequest);
    when(awsHttpClient.executePost(
            eq(BEGIN_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(nonRetryableFailureResponse);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse =
        HttpClientResponse.create(
            200,
            mapper.writeValueAsString(
                buildTransactionStatusResponse(
                    /* hasFailures= */ false,
                    /* isExpired= */ false,
                    "1682345560947309",
                    TransactionPhase.BEGIN)),
            responseHeaders);
    Map<String, String> statusRequestExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    statusRequestExpectedHeadersMap.remove(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY);
    when(awsHttpClient.executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap)))
        .thenReturn(statusResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionBegin(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.FAILURE);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  @Test
  public void performAction_begin_IOException() throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.BEGIN, transactionRequest);
    when(awsHttpClient.executePost(
            eq(BEGIN_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenThrow(new IOException());

    ExecutionResult executionResult = privacyBudgetClient.performActionBegin(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.RETRY);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  @Test
  public void performAction_prepare_success() throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    when(awsHttpClient.executePost(
            eq(PREPARE_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(successResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionPrepare(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performAction_commit_success() throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.COMMIT, transactionRequest);
    when(awsHttpClient.executePost(
            eq(COMMIT_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(successResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionCommit(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performAction_notify_success() throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.NOTIFY, transactionRequest);
    when(awsHttpClient.executePost(
            eq(NOTIFY_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(successResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionNotify(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performAction_abort_success() throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.ABORT, transactionRequest);
    when(awsHttpClient.executePost(
            eq(ABORT_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(successResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionAbort(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performAction_end_success() throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.END, transactionRequest);
    when(awsHttpClient.executePost(
            eq(END_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(successResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionEnd(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performAction_privacyBudgetExhausted()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    when(awsHttpClient.executePost(
            eq(PREPARE_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(budgetExhaustedResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionPrepare(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.FAILURE);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.UNKNOWN);
    assertThat(transaction.getExhaustedPrivacyBudgetUnits())
        .containsExactly(transactionRequest.privacyBudgetUnits().get(0));
  }

  @Test
  public void performAction_unauthenticated() throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    when(awsHttpClient.executePost(
            eq(PREPARE_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(unauthenticatedResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionPrepare(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.FAILURE);
    assertThat(executionResult.statusCode())
        .isEqualTo(StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHENTICATED);
  }

  @Test
  public void performAction_unauthorized() throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    when(awsHttpClient.executePost(
            eq(PREPARE_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(unauthorizedResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionPrepare(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.FAILURE);
    assertThat(executionResult.statusCode())
        .isEqualTo(StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHORIZED);
  }

  @Test
  public void performAction_begin_preConditionNotMet_statusCheck_phaseFailed()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.BEGIN, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse =
        HttpClientResponse.create(
            200,
            "{\"has_failures\":true,\"is_expired\":false,\"last_execution_timestamp\":1682345560947309"
                + ",\"transaction_execution_phase\":\"PREPARE\"}",
            responseHeaders);
    when(awsHttpClient.executePost(
            eq(BEGIN_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(preconditionNotMetFailureResponse);
    when(awsHttpClient.executeGet(
            eq(TRANSACTION_STATUS_URI), eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(statusResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionBegin(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.RETRY);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  @Test
  public void performAction_begin_preConditionNotMet_statusCheckFailed()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.BEGIN, transactionRequest);
    when(awsHttpClient.executePost(
            eq(BEGIN_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(preconditionNotMetFailureResponse);
    when(awsHttpClient.executeGet(
            eq(TRANSACTION_STATUS_URI), eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenThrow(new IOException("Timeout waiting for reply"));

    ExecutionResult executionResult = privacyBudgetClient.performActionBegin(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.FAILURE);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  @Test
  public void performAction_begin_preConditionNotMet_statusCheck_non200Response()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.BEGIN, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse = HttpClientResponse.create(400, "", responseHeaders);
    when(awsHttpClient.executePost(
            eq(BEGIN_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(preconditionNotMetFailureResponse);
    when(awsHttpClient.executeGet(
            eq(TRANSACTION_STATUS_URI), eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(statusResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionBegin(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.FAILURE);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  @Test
  public void performAction_begin_preConditionNotMet_statusCheck_transactionExpired()
      throws IOException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.BEGIN, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse =
        HttpClientResponse.create(
            200,
            mapper.writeValueAsString(
                buildTransactionStatusResponse(
                    /* hasFailures= */ false,
                    /* isExpired= */ true,
                    "1682345560947309",
                    TransactionPhase.PREPARE)),
            responseHeaders);
    when(awsHttpClient.executePost(
            eq(BEGIN_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(preconditionNotMetFailureResponse);
    when(awsHttpClient.executeGet(
            eq(TRANSACTION_STATUS_URI), eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(statusResponse);
    PrivacyBudgetClientException expectedException =
        new PrivacyBudgetClientException(
            StatusCode.TRANSACTION_ENGINE_TRANSACTION_IS_EXPIRED.name());

    PrivacyBudgetClientException actualException =
        assertThrows(
            PrivacyBudgetClientException.class,
            () -> privacyBudgetClient.performActionBegin(transaction));

    assertThat(expectedException.getMessage()).isEqualTo(actualException.getMessage());
  }

  @Test
  public void performAction_prepare_preConditionNotMet_statusCheck_incorrectTimestamp()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse =
        HttpClientResponse.create(
            200,
            mapper.writeValueAsString(
                buildTransactionStatusResponse(
                    /* hasFailures= */ false,
                    /* isExpired= */ false,
                    "1682345560947309",
                    TransactionPhase.PREPARE)),
            responseHeaders);
    Map<String, String> originalExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    Map<String, String> statusRequestExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    statusRequestExpectedHeadersMap.remove(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY);
    when(awsHttpClient.executePost(
            eq(PREPARE_PHASE_URI), eq(expectedPayload()), eq(originalExpectedHeadersMap)))
        .thenReturn(preconditionNotMetFailureResponse);
    when(awsHttpClient.executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap)))
        .thenReturn(statusResponse);

    ExecutionResult actualResult = privacyBudgetClient.performActionPrepare(transaction);

    verify(awsHttpClient, times(1))
        .executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap));
    verify(awsHttpClient, times(1))
        .executePost(eq(PREPARE_PHASE_URI), eq(expectedPayload()), eq(originalExpectedHeadersMap));
    assertThat(actualResult.executionStatus()).isEqualTo(ExecutionStatus.RETRY);
    assertThat(transaction.getLastExecutionTimestamp(ENDPOINT)).isEqualTo("1682345560947309");
  }

  @Test
  public void performAction_prepare_preConditionNotMet_statusCheck_serverAheadByOne()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse =
        HttpClientResponse.create(
            200,
            mapper.writeValueAsString(
                buildTransactionStatusResponse(
                    /* hasFailures= */ false,
                    /* isExpired= */ false,
                    "1682345560947309",
                    TransactionPhase.COMMIT)),
            responseHeaders);
    Map<String, String> originalExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    Map<String, String> statusRequestExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    statusRequestExpectedHeadersMap.remove(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY);
    when(awsHttpClient.executePost(
            eq(PREPARE_PHASE_URI), eq(expectedPayload()), eq(originalExpectedHeadersMap)))
        .thenReturn(preconditionNotMetFailureResponse);
    when(awsHttpClient.executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap)))
        .thenReturn(statusResponse);

    ExecutionResult actualResult = privacyBudgetClient.performActionPrepare(transaction);

    verify(awsHttpClient, times(1))
        .executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap));
    verify(awsHttpClient, times(1))
        .executePost(eq(PREPARE_PHASE_URI), eq(expectedPayload()), eq(originalExpectedHeadersMap));
    assertThat(actualResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(transaction.getLastExecutionTimestamp(ENDPOINT)).isEqualTo("1682345560947309");
  }

  @Test
  public void performAction_notify_preConditionNotMet_statusCheck_serverAheadByOne()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.NOTIFY, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse =
        HttpClientResponse.create(
            200,
            mapper.writeValueAsString(
                buildTransactionStatusResponse(
                    /* hasFailures= */ false,
                    /* isExpired= */ false,
                    "1682345560947309",
                    TransactionPhase.END)),
            responseHeaders);
    Map<String, String> originalExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    Map<String, String> statusRequestExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    statusRequestExpectedHeadersMap.remove(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY);
    when(awsHttpClient.executePost(
            eq(NOTIFY_PHASE_URI), eq(expectedPayload()), eq(originalExpectedHeadersMap)))
        .thenReturn(preconditionNotMetFailureResponse);
    when(awsHttpClient.executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap)))
        .thenReturn(statusResponse);

    ExecutionResult actualResult = privacyBudgetClient.performActionNotify(transaction);

    verify(awsHttpClient, times(1))
        .executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap));
    verify(awsHttpClient, times(1))
        .executePost(eq(NOTIFY_PHASE_URI), eq(expectedPayload()), eq(originalExpectedHeadersMap));
    assertThat(actualResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(transaction.getLastExecutionTimestamp(ENDPOINT)).isEqualTo("1682345560947309");
  }

  @Test
  public void performAction_abort_preConditionNotMet_statusCheck_serverAheadByOne()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.ABORT, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse =
        HttpClientResponse.create(
            200,
            mapper.writeValueAsString(
                buildTransactionStatusResponse(
                    /* hasFailures= */ false,
                    /* isExpired= */ false,
                    "1682345560947309",
                    TransactionPhase.END)),
            responseHeaders);
    Map<String, String> originalExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    Map<String, String> statusRequestExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    statusRequestExpectedHeadersMap.remove(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY);
    when(awsHttpClient.executePost(
            eq(ABORT_PHASE_URI), eq(expectedPayload()), eq(originalExpectedHeadersMap)))
        .thenReturn(preconditionNotMetFailureResponse);
    when(awsHttpClient.executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap)))
        .thenReturn(statusResponse);

    ExecutionResult actualResult = privacyBudgetClient.performActionAbort(transaction);

    verify(awsHttpClient, times(1))
        .executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap));
    verify(awsHttpClient, times(1))
        .executePost(eq(ABORT_PHASE_URI), eq(expectedPayload()), eq(originalExpectedHeadersMap));
    assertThat(actualResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(transaction.getLastExecutionTimestamp(ENDPOINT)).isEqualTo("1682345560947309");
  }

  @Test
  public void performAction_prepare_preConditionNotMet_statusCheck_serverAheadByTwo()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse =
        HttpClientResponse.create(
            200,
            mapper.writeValueAsString(
                buildTransactionStatusResponse(
                    /* hasFailures= */ false,
                    /* isExpired= */ false,
                    "1682345560947309",
                    TransactionPhase.NOTIFY)),
            responseHeaders);
    Map<String, String> originalExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    Map<String, String> statusRequestExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    statusRequestExpectedHeadersMap.remove(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY);
    when(awsHttpClient.executePost(
            eq(PREPARE_PHASE_URI), eq(expectedPayload()), eq(originalExpectedHeadersMap)))
        .thenReturn(preconditionNotMetFailureResponse);
    when(awsHttpClient.executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap)))
        .thenReturn(statusResponse);
    PrivacyBudgetClientException expectedException =
        new PrivacyBudgetClientException(
            "The PrivacyBudget client and server phases are out of sync. server phase"
                + " value: NOTIFY. client phase value: PREPARE. Transaction cannot be completed");

    PrivacyBudgetClientException actualException =
        assertThrows(
            PrivacyBudgetClientException.class,
            () -> privacyBudgetClient.performActionPrepare(transaction));

    assertThat(actualException.getMessage()).isEqualTo(expectedException.getMessage());
    verify(awsHttpClient, times(1))
        .executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap));
    verify(awsHttpClient, times(1))
        .executePost(eq(PREPARE_PHASE_URI), eq(expectedPayload()), eq(originalExpectedHeadersMap));
  }

  @Test
  public void performAction_commit_response400_statusCheck_phaseSucceeded()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.COMMIT, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse =
        HttpClientResponse.create(
            200,
            mapper.writeValueAsString(
                buildTransactionStatusResponse(
                    /* hasFailures= */ false,
                    /* isExpired= */ false,
                    "1682345560947309",
                    TransactionPhase.NOTIFY)),
            responseHeaders);
    when(awsHttpClient.executePost(
            eq(COMMIT_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(badRequestFailureResponse);
    Map<String, String> statusRequestExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    statusRequestExpectedHeadersMap.remove(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY);
    when(awsHttpClient.executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap)))
        .thenReturn(statusResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionCommit(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performAction_commit_Response400_statusCheck_phaseFailed()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.COMMIT, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse =
        HttpClientResponse.create(
            200,
            "{\"has_failures\":true,\"is_expired\":false,\"last_execution_timestamp\":1682345560947309"
                + ",\"transaction_execution_phase\":\"COMMIT\"}",
            responseHeaders);
    when(awsHttpClient.executePost(
            eq(COMMIT_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(badRequestFailureResponse);
    Map<String, String> statusRequestExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    statusRequestExpectedHeadersMap.remove(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY);
    when(awsHttpClient.executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap)))
        .thenReturn(statusResponse);

    ExecutionResult executionResult = privacyBudgetClient.performActionCommit(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.RETRY);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  @Test
  public void performAction_Prepare_Response400_statusCheck_ServerPhaseAheadBy2()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);

    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse =
        HttpClientResponse.create(
            200,
            mapper.writeValueAsString(
                buildTransactionStatusResponse(
                    /* hasFailures= */ false,
                    /* isExpired= */ false,
                    "1682345560947309",
                    TransactionPhase.NOTIFY)),
            responseHeaders);
    when(awsHttpClient.executePost(
            eq(PREPARE_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(badRequestFailureResponse);
    Map<String, String> statusRequestExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    statusRequestExpectedHeadersMap.remove(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY);
    when(awsHttpClient.executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap)))
        .thenReturn(statusResponse);
    PrivacyBudgetClientException expectedException =
        new PrivacyBudgetClientException(
            "The PrivacyBudget client and server phases are out of sync. server phase"
                + " value: NOTIFY. client phase value: PREPARE. Transaction cannot be completed");

    PrivacyBudgetClientException actualException =
        assertThrows(
            PrivacyBudgetClientException.class,
            () -> privacyBudgetClient.performActionPrepare(transaction));

    assertThat(actualException.getMessage()).isEqualTo(expectedException.getMessage());
  }

  @Test
  public void performAction_commit_Response400_statusCheck_statusCheckFailed()
      throws IOException, PrivacyBudgetClientException {
    Transaction transaction =
        generateTransaction(ENDPOINT, TRANSACTION_ID, TransactionPhase.COMMIT, transactionRequest);
    long lastExecTimeStamp = Instant.now().toEpochMilli();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, String.valueOf(lastExecTimeStamp));
    HttpClientResponse statusResponse =
        HttpClientResponse.create(
            200,
            "{\"has_failures\":false,\"is_expired\":false,\"last_execution_timestamp\":1682345560947309"
                + ",\"transaction_execution_phase\":\"NOTIFY\"}",
            responseHeaders);
    when(awsHttpClient.executePost(
            eq(COMMIT_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(ENDPOINT, transaction))))
        .thenReturn(badRequestFailureResponse);
    Map<String, String> statusRequestExpectedHeadersMap = expectedHeadersMap(ENDPOINT, transaction);
    statusRequestExpectedHeadersMap.remove(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY);
    when(awsHttpClient.executeGet(eq(TRANSACTION_STATUS_URI), eq(statusRequestExpectedHeadersMap)))
        .thenThrow(new IOException("Timeout waiting for reply"));

    ExecutionResult executionResult = privacyBudgetClient.performActionCommit(transaction);

    assertThat(executionResult.executionStatus()).isEqualTo(ExecutionStatus.FAILURE);
    assertThat(executionResult.statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  private static Transaction generateTransaction(
      String endpoint,
      UUID transactionId,
      TransactionPhase currentPhase,
      TransactionRequest transactionRequest) {
    Transaction transaction = new Transaction();
    transaction.setId(transactionId);
    transaction.setCurrentPhase(currentPhase);
    transaction.setRequest(transactionRequest);
    if (currentPhase != TransactionPhase.BEGIN) {
      transaction.setLastExecutionTimestamp(endpoint, String.valueOf(Instant.now().getNano()));
    }
    return transaction;
  }

  private static TransactionRequest generateTransactionRequest() {
    final Instant timeInstant1 = Instant.ofEpochMilli(1658960799);
    final Instant timeInstant2 = Instant.ofEpochMilli(1658960845);
    final PrivacyBudgetUnit unit1 =
        PrivacyBudgetUnit.builder()
            .privacyBudgetKey("budgetkey1")
            .reportingWindow(timeInstant1)
            .build();
    final PrivacyBudgetUnit unit2 =
        PrivacyBudgetUnit.builder()
            .privacyBudgetKey("budgetkey2")
            .reportingWindow(timeInstant2)
            .build();
    return TransactionRequest.builder()
        .setTransactionId(TRANSACTION_ID)
        .setPrivacyBudgetUnits(ImmutableList.of(unit1, unit2))
        .setTransactionSecret("transaction-secret")
        .setTimeout(Timestamp.from(Instant.now()))
        .setAttributionReportTo("dummy-reporting-origin")
        .build();
  }

  private static String expectedPayload() {
    return "{\"t\":[{\"reporting_time\":\"1970-01-20T04:49:20.799Z\",\"key\":\"budgetkey1\",\"token\":1},"
               + "{\"reporting_time\":\"1970-01-20T04:49:20.845Z\",\"key\":\"budgetkey2\",\"token\":1}],\"v\":\"1.0\"}";
  }

  private static Map<String, String> expectedHeadersMap(String endpoint, Transaction transaction) {
    Map<String, String> headers = new HashMap<>();
    headers.put("x-gscp-transaction-id", transaction.getId().toString().toUpperCase());
    headers.put("x-gscp-claimed-identity", "dummy-reporting-origin");
    headers.put("x-gscp-transaction-secret", "transaction-secret");
    if (transaction.getCurrentPhase() != TransactionPhase.BEGIN) {
      headers.put(
          TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY,
          transaction.getLastExecutionTimestamp(endpoint));
    }
    return headers;
  }

  private static TransactionStatusResponse buildTransactionStatusResponse(
      boolean hasFailures,
      boolean isExpired,
      String lastExecutionTimestamp,
      TransactionPhase currentPhase) {
    return TransactionStatusResponse.Builder.builder()
        .hasFailed(hasFailures)
        .isExpired(isExpired)
        .lastExecutionTimestamp(lastExecutionTimestamp)
        .currentPhase(currentPhase)
        .build();
  }
}
