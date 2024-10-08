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
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.common.truth.Expect;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.coordinator.privacy.budgeting.model.ReportingOriginToPrivacyBudgetUnits;
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
import java.util.stream.IntStream;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

public final class PrivacyBudgetClientV2Test {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Rule public final Expect expect = Expect.create();

  @Mock private HttpClientWithInterceptor awsHttpClient;

  private static final ObjectMapper mapper = new TimeObjectMapper();

  private static final UUID TRANSACTION_ID = UUID.randomUUID();
  private static final String ENDPOINT = "http://www.google.com/v1";

  private static final String HEALTH_CHECK_PHASE_URI = ENDPOINT + "/transactions:health-check";
  private static final String CONSUME_BUDGET_PHASE_URI = ENDPOINT + "/transactions:consume-budget";
  private static final String TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY =
      "x-gscp-transaction-last-execution-timestamp";
  private TransactionRequest transactionRequest;
  private HttpClientResponse successResponse;
  private HttpClientResponse retryableFailureResponse;
  private HttpClientResponse nonRetryableFailureResponse;

  private HttpClientResponse badRequestFailureResponse;
  private HttpClientResponse budgetExhaustedResponse;
  private HttpClientResponse unauthenticatedResponse;
  private HttpClientResponse unauthorizedResponse;

  private PrivacyBudgetClientImplV2 privacyBudgetClient;

  @Before
  public void setup() {
    this.privacyBudgetClient = new PrivacyBudgetClientImplV2(awsHttpClient, ENDPOINT);
    transactionRequest = generateTransactionRequest();
    Map<String, String> responseHeaders =
        ImmutableMap.of(
            TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY,
            String.valueOf(Instant.now().toEpochMilli()));
    successResponse = HttpClientResponse.create(200, "", responseHeaders);
    retryableFailureResponse = HttpClientResponse.create(500, "", Collections.emptyMap());
    nonRetryableFailureResponse = HttpClientResponse.create(400, "", Collections.emptyMap());
    badRequestFailureResponse = HttpClientResponse.create(400, "", Collections.emptyMap());
    budgetExhaustedResponse =
        HttpClientResponse.create(409, "{\"f\":[0],\"v\":\"1.0\"}", Collections.emptyMap());
    unauthenticatedResponse = HttpClientResponse.create(401, "", Collections.emptyMap());
    unauthorizedResponse = HttpClientResponse.create(403, "", Collections.emptyMap());
  }

  @Test
  public void healthCheckSuccess() throws IOException {
    when(awsHttpClient.executePost(
            eq(HEALTH_CHECK_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.HEALTH_CHECK, transactionRequest))))
        .thenReturn(successResponse);

    HealthCheckResponse healthCheckResponse =
        privacyBudgetClient.performActionHealthCheck(transactionRequest);

    expect
        .that(healthCheckResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.SUCCESS);
    expect.that(healthCheckResponse.executionResult().statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void healthCheckRetriableFailure() throws IOException {
    when(awsHttpClient.executePost(
            eq(HEALTH_CHECK_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.HEALTH_CHECK, transactionRequest))))
        .thenReturn(retryableFailureResponse);

    HealthCheckResponse healthCheckResponse =
        privacyBudgetClient.performActionHealthCheck(transactionRequest);

    expect
        .that(healthCheckResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.RETRY);
    expect.that(healthCheckResponse.executionResult().statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  @Test
  public void healthCheckBadRequestFailureResponse() throws IOException {
    when(awsHttpClient.executePost(
            eq(HEALTH_CHECK_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.HEALTH_CHECK, transactionRequest))))
        .thenReturn(badRequestFailureResponse);

    HealthCheckResponse healthCheckResponse =
        privacyBudgetClient.performActionHealthCheck(transactionRequest);

    expect
        .that(healthCheckResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.FAILURE);
    expect.that(healthCheckResponse.executionResult().statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  @Test
  public void healthCheckNonRetriableFailure() throws IOException {
    when(awsHttpClient.executePost(
            eq(HEALTH_CHECK_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.HEALTH_CHECK, transactionRequest))))
        .thenReturn(nonRetryableFailureResponse);

    HealthCheckResponse healthCheckResponse =
        privacyBudgetClient.performActionHealthCheck(transactionRequest);

    expect
        .that(healthCheckResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.FAILURE);
    expect.that(healthCheckResponse.executionResult().statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  /**
   * In a real scenario, generatePayload would be throwing the JsonProcessingException, not
   * executePost.
   */
  @Test
  public void healthCheckJsonProcessingException() throws IOException {
    when(awsHttpClient.executePost(
            eq(HEALTH_CHECK_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.HEALTH_CHECK, transactionRequest))))
        .thenThrow(new TestJsonProcessingException("Error parsing JSON."));

    HealthCheckResponse consumeBudgetResponse =
        privacyBudgetClient.performActionHealthCheck(transactionRequest);

    expect
        .that(consumeBudgetResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.RETRY);
    expect
        .that(consumeBudgetResponse.executionResult().statusCode())
        .isEqualTo(StatusCode.MALFORMED_DATA);
  }

  @Test
  public void healthCheckIOException() throws IOException {
    when(awsHttpClient.executePost(
            eq(HEALTH_CHECK_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.HEALTH_CHECK, transactionRequest))))
        .thenThrow(new IOException());

    HealthCheckResponse healthCheckResponse =
        privacyBudgetClient.performActionHealthCheck(transactionRequest);

    expect
        .that(healthCheckResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.RETRY);
    expect.that(healthCheckResponse.executionResult().statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  @Test
  public void consumeBudgetPrepareSuccess() throws IOException {
    when(awsHttpClient.executePost(
            eq(CONSUME_BUDGET_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.CONSUME_BUDGET, transactionRequest))))
        .thenReturn(successResponse);

    ConsumeBudgetResponse consumeBudgetResponse =
        privacyBudgetClient.performActionConsumeBudget(transactionRequest);

    expect
        .that(consumeBudgetResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.SUCCESS);
    expect.that(consumeBudgetResponse.executionResult().statusCode()).isEqualTo(StatusCode.OK);
    expect.that(consumeBudgetResponse.exhaustedPrivacyBudgetUnitsByOrigin()).isEmpty();
  }

  @Test
  public void consumeBudgetPrivacyBudgetExhausted() throws IOException {
    when(awsHttpClient.executePost(
            eq(CONSUME_BUDGET_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.CONSUME_BUDGET, transactionRequest))))
        .thenReturn(budgetExhaustedResponse);

    ConsumeBudgetResponse consumeBudgetResponse =
        privacyBudgetClient.performActionConsumeBudget(transactionRequest);

    PrivacyBudgetUnit exhaustedBudgetUnit =
        transactionRequest
            .reportingOriginToPrivacyBudgetUnitsList()
            .get(0)
            .privacyBudgetUnits()
            .get(0);
    ReportingOriginToPrivacyBudgetUnits exhaustedBudgetUnitsByOrigin =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("dummy-reporting-origin")
            .setPrivacyBudgetUnits(ImmutableList.of(exhaustedBudgetUnit))
            .build();

    expect
        .that(consumeBudgetResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.FAILURE);
    expect
        .that(consumeBudgetResponse.executionResult().statusCode())
        .isEqualTo(StatusCode.PRIVACY_BUDGET_CLIENT_BUDGET_EXHAUSTED);
    expect
        .that(consumeBudgetResponse.exhaustedPrivacyBudgetUnitsByOrigin())
        .containsExactly(exhaustedBudgetUnitsByOrigin);
  }

  @Test
  public void consumeBudgetUnauthenticated() throws IOException {
    when(awsHttpClient.executePost(
            eq(CONSUME_BUDGET_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.CONSUME_BUDGET, transactionRequest))))
        .thenReturn(unauthenticatedResponse);

    ConsumeBudgetResponse consumeBudgetResponse =
        privacyBudgetClient.performActionConsumeBudget(transactionRequest);

    expect
        .that(consumeBudgetResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.FAILURE);
    expect
        .that(consumeBudgetResponse.executionResult().statusCode())
        .isEqualTo(StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHENTICATED);
  }

  @Test
  public void consumeBudgetUnauthorized() throws IOException {
    when(awsHttpClient.executePost(
            eq(CONSUME_BUDGET_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.CONSUME_BUDGET, transactionRequest))))
        .thenReturn(unauthorizedResponse);

    ConsumeBudgetResponse consumeBudgetResponse =
        privacyBudgetClient.performActionConsumeBudget(transactionRequest);

    expect
        .that(consumeBudgetResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.FAILURE);
    expect
        .that(consumeBudgetResponse.executionResult().statusCode())
        .isEqualTo(StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHORIZED);
  }

  @Test
  public void consumeBudgetNonRetriableFailure() throws IOException {
    when(awsHttpClient.executePost(
            eq(CONSUME_BUDGET_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.CONSUME_BUDGET, transactionRequest))))
        .thenReturn(nonRetryableFailureResponse);

    ConsumeBudgetResponse healthCheckResponse =
        privacyBudgetClient.performActionConsumeBudget(transactionRequest);

    expect
        .that(healthCheckResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.FAILURE);
    expect.that(healthCheckResponse.executionResult().statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  /**
   * In a real scenario, generatePayload would be throwing the JsonProcessingException, not
   * executePost.
   */
  @Test
  public void consumeBudgetJsonProcessingException() throws IOException {
    when(awsHttpClient.executePost(
            eq(CONSUME_BUDGET_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.CONSUME_BUDGET, transactionRequest))))
        .thenThrow(new TestJsonProcessingException("Error parsing JSON."));

    ConsumeBudgetResponse consumeBudgetResponse =
        privacyBudgetClient.performActionConsumeBudget(transactionRequest);

    expect
        .that(consumeBudgetResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.RETRY);
    expect
        .that(consumeBudgetResponse.executionResult().statusCode())
        .isEqualTo(StatusCode.MALFORMED_DATA);
  }

  @Test
  public void consumeBudgetIOException() throws IOException {
    when(awsHttpClient.executePost(
            eq(CONSUME_BUDGET_PHASE_URI),
            eq(expectedPayload()),
            eq(expectedHeadersMap(TransactionPhase.CONSUME_BUDGET, transactionRequest))))
        .thenThrow(new IOException());

    ConsumeBudgetResponse consumeBudgetResponse =
        privacyBudgetClient.performActionConsumeBudget(transactionRequest);

    expect
        .that(consumeBudgetResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.RETRY);
    expect.that(consumeBudgetResponse.executionResult().statusCode()).isEqualTo(StatusCode.UNKNOWN);
  }

  @Test
  public void getExhaustedPrivacyBudgetUnitsByOriginLastUnitPerOrigin() {
    ImmutableList<PrivacyBudgetUnit> budgetUnits = getPrivacyBudgetUnits(7);
    ReportingOriginToPrivacyBudgetUnits origin1ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin1")
            .setPrivacyBudgetUnits(
                ImmutableList.of(budgetUnits.get(0), budgetUnits.get(1), budgetUnits.get(3)))
            .build();
    ReportingOriginToPrivacyBudgetUnits origin2ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin2")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(2), budgetUnits.get(5)))
            .build();
    ReportingOriginToPrivacyBudgetUnits origin3ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin3")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(4), budgetUnits.get(6)))
            .build();

    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin1ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin1")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(3)))
            .build();
    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin2ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin2")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(5)))
            .build();
    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin3ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin3")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(6)))
            .build();

    ImmutableList<Integer> budgetExhaustedIndices = ImmutableList.of(2, 4, 6);

    ImmutableList<ReportingOriginToPrivacyBudgetUnits> exhaustedBudgetUnitsByOrigin =
        privacyBudgetClient.getExhaustedPrivacyBudgetUnitsByOrigin(
            ImmutableList.of(origin1ToUnits, origin2ToUnits, origin3ToUnits),
            budgetExhaustedIndices);
    expect
        .that(exhaustedBudgetUnitsByOrigin)
        .containsExactly(
            expectedExhaustedOrigin1ToUnits,
            expectedExhaustedOrigin2ToUnits,
            expectedExhaustedOrigin3ToUnits);
  }

  @Test
  public void getExhaustedPrivacyBudgetUnitsByOriginMixOfMidAndLastBudgetsForOrigins() {
    ImmutableList<PrivacyBudgetUnit> budgetUnits = getPrivacyBudgetUnits(10);
    ReportingOriginToPrivacyBudgetUnits origin1ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin1")
            .setPrivacyBudgetUnits(
                ImmutableList.of(
                    budgetUnits.get(0), budgetUnits.get(1), budgetUnits.get(3), budgetUnits.get(4)))
            .build();
    ReportingOriginToPrivacyBudgetUnits origin2ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin2")
            .setPrivacyBudgetUnits(
                ImmutableList.of(budgetUnits.get(2), budgetUnits.get(5), budgetUnits.get(7)))
            .build();
    ReportingOriginToPrivacyBudgetUnits origin3ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin3")
            .setPrivacyBudgetUnits(
                ImmutableList.of(budgetUnits.get(6), budgetUnits.get(8), budgetUnits.get(9)))
            .build();

    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin1ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin1")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(3)))
            .build();
    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin2ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin2")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(5)))
            .build();
    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin3ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin3")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(9)))
            .build();

    ImmutableList<Integer> budgetExhaustedIndices = ImmutableList.of(2, 5, 9);

    ImmutableList<ReportingOriginToPrivacyBudgetUnits> exhaustedBudgetUnitsByOrigin =
        privacyBudgetClient.getExhaustedPrivacyBudgetUnitsByOrigin(
            ImmutableList.of(origin1ToUnits, origin2ToUnits, origin3ToUnits),
            budgetExhaustedIndices);
    expect
        .that(exhaustedBudgetUnitsByOrigin)
        .containsExactly(
            expectedExhaustedOrigin1ToUnits,
            expectedExhaustedOrigin2ToUnits,
            expectedExhaustedOrigin3ToUnits);
  }

  @Test
  public void getExhaustedPrivacyBudgetUnitsByOriginMixOfFirstAndLastBudgetsForOrigins() {
    ImmutableList<PrivacyBudgetUnit> budgetUnits = getPrivacyBudgetUnits(10);
    ReportingOriginToPrivacyBudgetUnits origin1ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin1")
            .setPrivacyBudgetUnits(
                ImmutableList.of(
                    budgetUnits.get(0), budgetUnits.get(1), budgetUnits.get(3), budgetUnits.get(4)))
            .build();
    ReportingOriginToPrivacyBudgetUnits origin2ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin2")
            .setPrivacyBudgetUnits(
                ImmutableList.of(budgetUnits.get(2), budgetUnits.get(5), budgetUnits.get(7)))
            .build();
    ReportingOriginToPrivacyBudgetUnits origin3ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin3")
            .setPrivacyBudgetUnits(
                ImmutableList.of(budgetUnits.get(6), budgetUnits.get(8), budgetUnits.get(9)))
            .build();

    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin1ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin1")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(0), budgetUnits.get(4)))
            .build();
    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin2ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin2")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(2)))
            .build();
    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin3ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin3")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(9)))
            .build();

    ImmutableList<Integer> budgetExhaustedIndices = ImmutableList.of(0, 3, 4, 9);

    ImmutableList<ReportingOriginToPrivacyBudgetUnits> exhaustedBudgetUnitsByOrigin =
        privacyBudgetClient.getExhaustedPrivacyBudgetUnitsByOrigin(
            ImmutableList.of(origin1ToUnits, origin2ToUnits, origin3ToUnits),
            budgetExhaustedIndices);
    expect
        .that(exhaustedBudgetUnitsByOrigin)
        .containsExactly(
            expectedExhaustedOrigin1ToUnits,
            expectedExhaustedOrigin2ToUnits,
            expectedExhaustedOrigin3ToUnits);
  }

  @Test
  public void getExhaustedPrivacyBudgetUnitsByOriginMixOfFirstLastAndMidBudgetsForOrigins() {
    ImmutableList<PrivacyBudgetUnit> budgetUnits = getPrivacyBudgetUnits(21);
    ReportingOriginToPrivacyBudgetUnits origin1ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin1")
            .setPrivacyBudgetUnits(
                ImmutableList.of(
                    budgetUnits.get(0),
                    budgetUnits.get(3),
                    budgetUnits.get(6),
                    budgetUnits.get(7),
                    budgetUnits.get(15)))
            .build();
    ReportingOriginToPrivacyBudgetUnits origin2ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin2")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(19), budgetUnits.get(20)))
            .build();
    ReportingOriginToPrivacyBudgetUnits origin3ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin3")
            .setPrivacyBudgetUnits(
                ImmutableList.of(
                    budgetUnits.get(1),
                    budgetUnits.get(4),
                    budgetUnits.get(8),
                    budgetUnits.get(11),
                    budgetUnits.get(16)))
            .build();
    ReportingOriginToPrivacyBudgetUnits origin4ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin4")
            .setPrivacyBudgetUnits(
                ImmutableList.of(
                    budgetUnits.get(2),
                    budgetUnits.get(9),
                    budgetUnits.get(12),
                    budgetUnits.get(17)))
            .build();
    ReportingOriginToPrivacyBudgetUnits origin5ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin5")
            .setPrivacyBudgetUnits(
                ImmutableList.of(
                    budgetUnits.get(5),
                    budgetUnits.get(10),
                    budgetUnits.get(13),
                    budgetUnits.get(14),
                    budgetUnits.get(18)))
            .build();

    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin1ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin1")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(0), budgetUnits.get(7)))
            .build();
    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin3ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin3")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(8), budgetUnits.get(16)))
            .build();
    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin4ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin4")
            .setPrivacyBudgetUnits(ImmutableList.of(budgetUnits.get(2), budgetUnits.get(17)))
            .build();
    ReportingOriginToPrivacyBudgetUnits expectedExhaustedOrigin5ToUnits =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("origin5")
            .setPrivacyBudgetUnits(
                ImmutableList.of(budgetUnits.get(10), budgetUnits.get(13), budgetUnits.get(14)))
            .build();

    ImmutableList<Integer> budgetExhaustedIndices =
        ImmutableList.of(0, 3, 9, 11, 12, 15, 17, 18, 19);

    ImmutableList<ReportingOriginToPrivacyBudgetUnits> exhaustedBudgetUnitsByOrigin =
        privacyBudgetClient.getExhaustedPrivacyBudgetUnitsByOrigin(
            ImmutableList.of(
                origin1ToUnits, origin2ToUnits, origin3ToUnits, origin4ToUnits, origin5ToUnits),
            budgetExhaustedIndices);
    expect
        .that(exhaustedBudgetUnitsByOrigin)
        .containsExactly(
            expectedExhaustedOrigin1ToUnits,
            expectedExhaustedOrigin3ToUnits,
            expectedExhaustedOrigin4ToUnits,
            expectedExhaustedOrigin5ToUnits);
  }

  private static ImmutableList<PrivacyBudgetUnit> getPrivacyBudgetUnits(int count) {
    final Instant timeInstant = Instant.ofEpochMilli(1658960799);
    return IntStream.range(0, count)
        .mapToObj(
            index ->
                PrivacyBudgetUnit.builder()
                    .privacyBudgetKey("budgetKey" + index)
                    .reportingWindow(timeInstant)
                    .build())
        .collect(toImmutableList());
  }

  private static TransactionRequest generateTransactionRequest() {
    final Instant timeInstant1 = Instant.ofEpochMilli(1658960799);
    final Instant timeInstant2 = Instant.ofEpochMilli(1658960845);
    final Instant timeInstant3 = Instant.ofEpochMilli(1658960891);
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
    final PrivacyBudgetUnit unit3 =
        PrivacyBudgetUnit.builder()
            .privacyBudgetKey("budgetkey3")
            .reportingWindow(timeInstant3)
            .build();
    return TransactionRequest.builder()
        .setTransactionId(TRANSACTION_ID)
        .setReportingOriginToPrivacyBudgetUnitsList(
            ImmutableList.of(
                ReportingOriginToPrivacyBudgetUnits.builder()
                    .setPrivacyBudgetUnits(ImmutableList.of(unit1, unit2))
                    .setReportingOrigin("dummy-reporting-origin")
                    .build(),
                ReportingOriginToPrivacyBudgetUnits.builder()
                    .setPrivacyBudgetUnits(ImmutableList.of(unit3))
                    .setReportingOrigin("other-reporting-origin")
                    .build()))
        .setTransactionSecret("transaction-secret")
        .setTimeout(Timestamp.from(Instant.now()))
        .setClaimedIdentity("dummy-reporting-site")
        .setTrustedServicesClientVersion("dummy-version")
        .build();
  }

  private static String expectedPayload() {
    return "{\"data\":["
        + "{\"keys\":["
        + "{\"reporting_time\":\"1970-01-20T04:49:20.799Z\",\"key\":\"budgetkey1\",\"token\":1},"
        + "{\"reporting_time\":\"1970-01-20T04:49:20.845Z\",\"key\":\"budgetkey2\",\"token\":1}],"
        + "\"reporting_origin\":\"dummy-reporting-origin\"},"
        + "{\"keys\":["
        + "{\"reporting_time\":\"1970-01-20T04:49:20.891Z\",\"key\":\"budgetkey3\",\"token\":1}],"
        + "\"reporting_origin\":\"other-reporting-origin\"}],\"v\":\"2.0\"}";
  }

  private static Map<String, String> expectedHeadersMap(
      TransactionPhase currentPhase, TransactionRequest transactionRequest) {
    Map<String, String> headers = new HashMap<>();
    headers.put(
        "x-gscp-transaction-id", transactionRequest.transactionId().toString().toUpperCase());
    headers.put("x-gscp-claimed-identity", "dummy-reporting-site");
    headers.put("x-gscp-transaction-secret", "transaction-secret");
    if (currentPhase != TransactionPhase.BEGIN && currentPhase != TransactionPhase.HEALTH_CHECK) {
      headers.put(TRANSACTION_LAST_EXEC_TIMESTAMP_HEADER_KEY, "1234");
    }
    return headers;
  }

  /**
   * JsonProcessingException has protected access, so we use our own TestJsonProcessingException
   * extended from JsonProcessingException for testing.
   */
  public static class TestJsonProcessingException extends JsonProcessingException {
    public TestJsonProcessingException(String message) {
      super(message);
    }
  }
}
