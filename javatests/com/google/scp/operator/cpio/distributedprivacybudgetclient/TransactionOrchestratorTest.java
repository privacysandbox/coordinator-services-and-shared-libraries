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

import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.common.collect.ImmutableList;
import com.google.common.truth.Expect;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.coordinator.privacy.budgeting.model.ReportingOriginToPrivacyBudgetUnits;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionOrchestrator.TransactionOrchestratorException;
import java.sql.Timestamp;
import java.time.Instant;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

public final class TransactionOrchestratorTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Rule public final Expect expect = Expect.create();
  @Mock private PrivacyBudgetClientV2 privacyBudgetClient1;
  @Mock private PrivacyBudgetClientV2 privacyBudgetClient2;

  private TransactionRequest transactionRequest;
  private TransactionOrchestratorImpl transactionOrchestrator;
  private static final UUID TRANSACTION_ID = UUID.randomUUID();

  @Before
  public void setUp() {
    transactionOrchestrator =
        new TransactionOrchestratorImpl(
            ImmutableList.of(privacyBudgetClient1, privacyBudgetClient2));
    transactionRequest = generateTransactionRequest();
  }

  @Test
  public void healthCheckAndConsumeBudgetAllSuccessful() throws TransactionOrchestratorException {
    when(privacyBudgetClient1.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());
    when(privacyBudgetClient2.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());

    when(privacyBudgetClient1.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseSuccess());
    when(privacyBudgetClient2.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseSuccess());

    ImmutableList<ReportingOriginToPrivacyBudgetUnits> result =
        transactionOrchestrator.execute(transactionRequest);

    verify(privacyBudgetClient1).performActionHealthCheck(transactionRequest);
    verify(privacyBudgetClient2).performActionHealthCheck(transactionRequest);
    verify(privacyBudgetClient1).performActionConsumeBudget(transactionRequest);
    verify(privacyBudgetClient2).performActionConsumeBudget(transactionRequest);
    expect.that(result).isEmpty();
  }

  @Test
  public void firstHealthCheckFailureConsumeBudgetSuccessful() {
    when(privacyBudgetClient1.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseFailure());

    TransactionOrchestratorException transactionOrchestratorException =
        assertThrows(
            TransactionOrchestratorException.class,
            () -> transactionOrchestrator.execute(transactionRequest));
    verify(privacyBudgetClient2, times(0)).performActionHealthCheck(transactionRequest);
    verify(privacyBudgetClient1, times(0)).performActionConsumeBudget(transactionRequest);
    verify(privacyBudgetClient2, times(0)).performActionConsumeBudget(transactionRequest);
    expect
        .that(transactionOrchestratorException.getStatusCode())
        .isEqualTo(StatusCode.TRANSACTION_ORCHESTRATOR_HEALTH_CHECK_FAILURE);
  }

  @Test
  public void secondHealthCheckFailureConsumeBudgetSuccessful() {
    when(privacyBudgetClient1.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());
    when(privacyBudgetClient2.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseFailure());

    TransactionOrchestratorException transactionOrchestratorException =
        assertThrows(
            TransactionOrchestratorException.class,
            () -> transactionOrchestrator.execute(transactionRequest));

    verify(privacyBudgetClient1, times(0)).performActionConsumeBudget(transactionRequest);
    verify(privacyBudgetClient2, times(0)).performActionConsumeBudget(transactionRequest);
    expect
        .that(transactionOrchestratorException.getStatusCode())
        .isEqualTo(StatusCode.TRANSACTION_ORCHESTRATOR_HEALTH_CHECK_FAILURE);
  }

  @Test
  public void HealthCheckSuccessfulFirstConsumeBudgetFailure() {
    when(privacyBudgetClient1.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());
    when(privacyBudgetClient2.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());

    when(privacyBudgetClient1.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseFailure());

    TransactionOrchestratorException transactionOrchestratorException =
        assertThrows(
            TransactionOrchestratorException.class,
            () -> transactionOrchestrator.execute(transactionRequest));
    verify(privacyBudgetClient2, times(0)).performActionConsumeBudget(transactionRequest);
    expect
        .that(transactionOrchestratorException.getStatusCode())
        .isEqualTo(StatusCode.TRANSACTION_ORCHESTRATOR_CONSUME_BUDGET_FAILURE);
  }

  @Test
  public void HealthCheckSuccessfulSecondConsumeBudgetFailure() {
    when(privacyBudgetClient1.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());
    when(privacyBudgetClient2.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());

    when(privacyBudgetClient1.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseSuccess());
    when(privacyBudgetClient2.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseFailure());

    TransactionOrchestratorException transactionOrchestratorException =
        assertThrows(
            TransactionOrchestratorException.class,
            () -> transactionOrchestrator.execute(transactionRequest));
    expect
        .that(transactionOrchestratorException.getStatusCode())
        .isEqualTo(StatusCode.TRANSACTION_ORCHESTRATOR_CONSUME_BUDGET_FAILURE);
  }

  @Test
  public void healthCheckSuccessfulFirstConsumeBudgetExhausted()
      throws TransactionOrchestratorException {
    when(privacyBudgetClient1.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());
    when(privacyBudgetClient2.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());

    when(privacyBudgetClient1.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseBudgetExhausted(transactionRequest));
    when(privacyBudgetClient2.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseSuccess());

    ImmutableList<ReportingOriginToPrivacyBudgetUnits> exhaustedBudget =
        transactionOrchestrator.execute(transactionRequest);
    expect
        .that(exhaustedBudget)
        .containsExactlyElementsIn(transactionRequest.reportingOriginToPrivacyBudgetUnitsList());
  }

  @Test
  public void healthCheckSuccessfulSecondConsumeBudgetExhausted()
      throws TransactionOrchestratorException {
    when(privacyBudgetClient1.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());
    when(privacyBudgetClient2.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());

    when(privacyBudgetClient1.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseSuccess());
    when(privacyBudgetClient2.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseBudgetExhausted(transactionRequest));

    ImmutableList<ReportingOriginToPrivacyBudgetUnits> exhaustedBudget =
        transactionOrchestrator.execute(transactionRequest);
    expect
        .that(exhaustedBudget)
        .containsExactlyElementsIn(transactionRequest.reportingOriginToPrivacyBudgetUnitsList());
  }

  @Test
  public void healthCheckSuccessfulAllConsumeBudgetExhausted()
      throws TransactionOrchestratorException {
    when(privacyBudgetClient1.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());
    when(privacyBudgetClient2.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());

    when(privacyBudgetClient1.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseBudgetExhausted(transactionRequest));
    when(privacyBudgetClient2.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseBudgetExhausted(transactionRequest));

    ImmutableList<ReportingOriginToPrivacyBudgetUnits> exhaustedBudget =
        transactionOrchestrator.execute(transactionRequest);

    // Ensures that budget is still consumed in the second coordinator when the first coordinator
    // returns budget exhausted.
    verify(privacyBudgetClient1).performActionConsumeBudget(transactionRequest);
    verify(privacyBudgetClient2).performActionConsumeBudget(transactionRequest);

    expect
        .that(exhaustedBudget)
        .containsExactlyElementsIn(transactionRequest.reportingOriginToPrivacyBudgetUnitsList());
  }

  /** If two coordinators differ on what budget is exhausted, we arbitrarily pick the first one. */
  @Test
  public void healthCheckSuccessfulConsumeBudgetCoordinatorsOutOfSync()
      throws TransactionOrchestratorException {
    when(privacyBudgetClient1.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());
    when(privacyBudgetClient2.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());

    ConsumeBudgetResponse onlyFirstReportingOriginToPbuExhaustedResponse =
        ConsumeBudgetResponse.builder()
            .setExecutionResult(
                ExecutionResult.create(
                    ExecutionStatus.FAILURE, StatusCode.PRIVACY_BUDGET_CLIENT_BUDGET_EXHAUSTED))
            .setExhaustedPrivacyBudgetUnitsByOrigin(
                ImmutableList.of(
                    transactionRequest.reportingOriginToPrivacyBudgetUnitsList().get(0)))
            .build();

    when(privacyBudgetClient1.performActionConsumeBudget(transactionRequest))
        .thenReturn(onlyFirstReportingOriginToPbuExhaustedResponse);
    when(privacyBudgetClient2.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseBudgetExhausted(transactionRequest));

    ImmutableList<ReportingOriginToPrivacyBudgetUnits> exhaustedBudget =
        transactionOrchestrator.execute(transactionRequest);
    expect
        .that(exhaustedBudget)
        .containsExactlyElementsIn(
            onlyFirstReportingOriginToPbuExhaustedResponse.exhaustedPrivacyBudgetUnitsByOrigin());
  }

  @Test
  public void healthCheckSuccessfulFirstConsumeBudgetBudgetExhaustedSecondConsumeBudgetFailure() {
    when(privacyBudgetClient1.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());
    when(privacyBudgetClient2.performActionHealthCheck(transactionRequest))
        .thenReturn(createHealthCheckResponseSuccess());

    when(privacyBudgetClient1.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseBudgetExhausted(transactionRequest));
    when(privacyBudgetClient2.performActionConsumeBudget(transactionRequest))
        .thenReturn(createConsumeBudgetResponseFailure());

    TransactionOrchestratorException transactionOrchestratorException =
        assertThrows(
            TransactionOrchestratorException.class,
            () -> transactionOrchestrator.execute(transactionRequest));
    expect
        .that(transactionOrchestratorException.getStatusCode())
        .isEqualTo(StatusCode.TRANSACTION_ORCHESTRATOR_CONSUME_BUDGET_FAILURE);
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
        .setTrustedServicesClientVersion("dummy-version")
        .setClaimedIdentity("dummy-reporting-site")
        .build();
  }

  private static HealthCheckResponse createHealthCheckResponseSuccess() {
    return HealthCheckResponse.builder()
        .setExecutionResult(ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK))
        .build();
  }

  private static HealthCheckResponse createHealthCheckResponseFailure() {
    return HealthCheckResponse.builder()
        .setExecutionResult(ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN))
        .build();
  }

  private static HealthCheckResponse createHealthCheckResponseRetry() {
    return HealthCheckResponse.builder()
        .setExecutionResult(ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN))
        .build();
  }

  private static ConsumeBudgetResponse createConsumeBudgetResponseSuccess() {
    return ConsumeBudgetResponse.builder()
        .setExecutionResult(ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK))
        .setExhaustedPrivacyBudgetUnitsByOrigin(ImmutableList.of())
        .build();
  }

  private static ConsumeBudgetResponse createConsumeBudgetResponseFailure() {
    return ConsumeBudgetResponse.builder()
        .setExecutionResult(ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN))
        .setExhaustedPrivacyBudgetUnitsByOrigin(ImmutableList.of())
        .build();
  }

  private static ConsumeBudgetResponse createConsumeBudgetResponseBudgetExhausted(
      TransactionRequest transactionRequest) {
    return ConsumeBudgetResponse.builder()
        .setExecutionResult(
            ExecutionResult.create(
                ExecutionStatus.FAILURE, StatusCode.PRIVACY_BUDGET_CLIENT_BUDGET_EXHAUSTED))
        .setExhaustedPrivacyBudgetUnitsByOrigin(
            transactionRequest.reportingOriginToPrivacyBudgetUnitsList())
        .build();
  }

  private static ConsumeBudgetResponse createConsumeBudgetResponseRetry() {
    return ConsumeBudgetResponse.builder()
        .setExecutionResult(ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN))
        .setExhaustedPrivacyBudgetUnitsByOrigin(ImmutableList.of())
        .build();
  }
}
