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
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import com.google.common.collect.ImmutableList;
import com.google.common.truth.Expect;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.coordinator.privacy.budgeting.model.ReportingOriginToPrivacyBudgetUnits;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClient.DistributedPrivacyBudgetClientException;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClient.DistributedPrivacyBudgetServiceException;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionEngine.TransactionEngineException;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionManager.TransactionManagerException;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionOrchestrator.TransactionOrchestratorException;
import com.google.testing.junit.testparameterinjector.TestParameter;
import com.google.testing.junit.testparameterinjector.TestParameterInjector;
import java.time.Instant;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(TestParameterInjector.class)
public final class DistributedPrivacyBudgetClientTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Rule public final Expect expect = Expect.create();
  @Mock private TransactionManagerImpl transactionManager;
  @Mock private TransactionOrchestratorImpl transactionOrchestrator;
  private DistributedPrivacyBudgetClientImpl distributedPrivacyBudgetClient;

  Instant testInstant = Instant.now();

  private enum EnableNewClientOptions {
    UNSET,
    ON,
    OFF
  }

  @TestParameter({"UNSET", "OFF"})
  private EnableNewClientOptions newClientOff;

  @TestParameter private EnableNewClientOptions enableNewClientOptionsAll;

  @Before
  public void setUp() {
    distributedPrivacyBudgetClient =
        new DistributedPrivacyBudgetClientImpl(transactionManager, transactionOrchestrator);
  }

  @Test
  public void consumePrivacyBudget_exhaustedBudget() throws Exception {
    var argument = ArgumentCaptor.forClass(TransactionRequest.class);
    PrivacyBudgetUnit exhaustedUnit =
        PrivacyBudgetUnit.builder()
            .privacyBudgetKey("privacybudgetkey")
            .reportingWindow(testInstant)
            .build();
    ImmutableList<PrivacyBudgetUnit> exhaustedUnits = ImmutableList.of(exhaustedUnit);
    ReportingOriginToPrivacyBudgetUnits exhaustedUnitsByOrigin =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("dummy-origin")
            .setPrivacyBudgetUnits(exhaustedUnits)
            .build();
    when(transactionManager.execute(any(TransactionRequest.class)))
        .thenReturn(ImmutableList.of(exhaustedUnitsByOrigin));

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        distributedPrivacyBudgetClient.consumePrivacyBudget(
            getConsumePrivacyBudgetRequest(1, newClientOff));

    verify(transactionManager).execute(argument.capture());
    assertThat(consumePrivacyBudgetResponse).isEqualTo(getExhaustedBudget());
  }

  @Test
  public void consumePrivacyBudget_nonExhaustedBudget() throws Exception {
    var argument = ArgumentCaptor.forClass(TransactionRequest.class);
    when(transactionManager.execute(any(TransactionRequest.class)))
        .thenReturn(ImmutableList.<ReportingOriginToPrivacyBudgetUnits>builder().build());

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        distributedPrivacyBudgetClient.consumePrivacyBudget(
            getConsumePrivacyBudgetRequest(1, newClientOff));

    verify(transactionManager).execute(argument.capture());
    assertThat(consumePrivacyBudgetResponse).isEqualTo(getNotExhaustedBudget());
  }

  @Test
  public void consumePrivacyBudget_overSizeLimit() {
    assertThrows(
        DistributedPrivacyBudgetClientException.class,
        () ->
            distributedPrivacyBudgetClient.consumePrivacyBudget(
                getConsumePrivacyBudgetRequest(30001, enableNewClientOptionsAll)));
  }

  @Test
  public void consumePrivacyBudget_transactionManagerFailure() throws Exception {
    doThrow(
            new TransactionManagerException(
                StatusCode.UNKNOWN,
                new TransactionEngineException(StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHENTICATED)))
        .when(transactionManager)
        .execute(any(TransactionRequest.class));
    var argument = ArgumentCaptor.forClass(TransactionRequest.class);

    DistributedPrivacyBudgetServiceException exception =
        assertThrows(
            DistributedPrivacyBudgetServiceException.class,
            () ->
                distributedPrivacyBudgetClient.consumePrivacyBudget(
                    getConsumePrivacyBudgetRequest(1, newClientOff)));
    verify(transactionManager).execute(argument.capture());
    assertThat(exception.getMessage()).isEqualTo(StatusCode.UNKNOWN.name());
    assertThat(exception.getCause().getCause().getMessage())
        .isEqualTo(StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHENTICATED.name());
  }

  @Test
  public void consumePrivacyBudgetNewClient_callsTransactionOrchestrator_success()
      throws Exception {
    ConsumePrivacyBudgetRequest consumePrivacyBudgetRequest =
        getConsumePrivacyBudgetRequest(/* numberOfPbus= */ 3, EnableNewClientOptions.ON);
    when(transactionOrchestrator.execute(any(TransactionRequest.class)))
        .thenReturn(ImmutableList.of());

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        distributedPrivacyBudgetClient.consumePrivacyBudget(consumePrivacyBudgetRequest);

    verify(transactionOrchestrator, times(1)).execute(any(TransactionRequest.class));
    verifyNoInteractions(transactionManager);

    expect.that(consumePrivacyBudgetResponse).isEqualTo(getNotExhaustedBudget());
  }

  @Test
  public void consumePrivacyBudgetNewClient_callsTransactionOrchestrator_budgetExhausted()
      throws Exception {
    ConsumePrivacyBudgetRequest consumePrivacyBudgetRequest =
        getConsumePrivacyBudgetRequest(/* numberOfPbus= */ 3, EnableNewClientOptions.ON);
    ImmutableList<ReportingOriginToPrivacyBudgetUnits> exhaustedBudget =
        consumePrivacyBudgetRequest.reportingOriginToPrivacyBudgetUnitsList();
    when(transactionOrchestrator.execute(any(TransactionRequest.class)))
        .thenReturn(exhaustedBudget);

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        distributedPrivacyBudgetClient.consumePrivacyBudget(consumePrivacyBudgetRequest);

    verify(transactionOrchestrator, times(1)).execute(any(TransactionRequest.class));
    verifyNoInteractions(transactionManager);

    expect
        .that(consumePrivacyBudgetResponse)
        .isEqualTo(
            ConsumePrivacyBudgetResponse.builder()
                .exhaustedPrivacyBudgetUnitsByOrigin(exhaustedBudget)
                .build());
  }

  @Test
  public void
      consumePrivacyBudgetNewClient_catchesTransactionOrchestratorExceptionWithHealthCheckFailure()
          throws Exception {
    ConsumePrivacyBudgetRequest consumePrivacyBudgetRequest =
        getConsumePrivacyBudgetRequest(/* numberOfPbus= */ 3, EnableNewClientOptions.ON);
    when(transactionOrchestrator.execute(any(TransactionRequest.class)))
        .thenThrow(
            new TransactionOrchestratorException(
                StatusCode.TRANSACTION_ORCHESTRATOR_HEALTH_CHECK_FAILURE));

    DistributedPrivacyBudgetServiceException distributedPrivacyBudgetServiceException =
        assertThrows(
            DistributedPrivacyBudgetServiceException.class,
            () -> distributedPrivacyBudgetClient.consumePrivacyBudget(consumePrivacyBudgetRequest));

    verifyNoInteractions(transactionManager);
    expect
        .that(distributedPrivacyBudgetServiceException.getStatusCode())
        .isEqualTo(StatusCode.TRANSACTION_ORCHESTRATOR_HEALTH_CHECK_FAILURE);
    expect
        .that(distributedPrivacyBudgetServiceException.getCause())
        .isInstanceOf(TransactionOrchestratorException.class);
  }

  @Test
  public void
      consumePrivacyBudgetNewClient_catchesTransactionOrchestratorExceptionWithConsumeBudgetFailure()
          throws Exception {
    ConsumePrivacyBudgetRequest consumePrivacyBudgetRequest =
        getConsumePrivacyBudgetRequest(/* numberOfPbus= */ 3, EnableNewClientOptions.ON);
    when(transactionOrchestrator.execute(any(TransactionRequest.class)))
        .thenThrow(
            new TransactionOrchestratorException(
                StatusCode.TRANSACTION_ORCHESTRATOR_CONSUME_BUDGET_FAILURE));

    DistributedPrivacyBudgetServiceException distributedPrivacyBudgetServiceException =
        assertThrows(
            DistributedPrivacyBudgetServiceException.class,
            () -> distributedPrivacyBudgetClient.consumePrivacyBudget(consumePrivacyBudgetRequest));

    verifyNoInteractions(transactionManager);
    expect
        .that(distributedPrivacyBudgetServiceException.getStatusCode())
        .isEqualTo(StatusCode.TRANSACTION_ORCHESTRATOR_CONSUME_BUDGET_FAILURE);
    expect
        .that(distributedPrivacyBudgetServiceException.getCause())
        .isInstanceOf(TransactionOrchestratorException.class);
  }

  private ConsumePrivacyBudgetResponse getExhaustedBudget() {
    // TODO: Add exhausted privacy budget key once the actual implementation
    // is complete with Http client integration
    ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits =
        ImmutableList.<PrivacyBudgetUnit>builder()
            .add(
                PrivacyBudgetUnit.builder()
                    .privacyBudgetKey("privacybudgetkey")
                    .reportingWindow(testInstant)
                    .build())
            .build();
    ReportingOriginToPrivacyBudgetUnits exhaustedUnitsByOrigin =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("dummy-origin")
            .setPrivacyBudgetUnits(privacyBudgetUnits)
            .build();
    return ConsumePrivacyBudgetResponse.builder()
        .exhaustedPrivacyBudgetUnitsByOrigin(ImmutableList.of(exhaustedUnitsByOrigin))
        .build();
  }

  private static ConsumePrivacyBudgetResponse getNotExhaustedBudget() {
    return ConsumePrivacyBudgetResponse.builder()
        .exhaustedPrivacyBudgetUnitsByOrigin(
            ImmutableList.<ReportingOriginToPrivacyBudgetUnits>builder().build())
        .build();
  }

  private ConsumePrivacyBudgetRequest getConsumePrivacyBudgetRequest(
      int numberOfPbus, EnableNewClientOptions enableNewClientOptions) {
    ImmutableList.Builder<PrivacyBudgetUnit> builder = ImmutableList.builder();
    for (int i = 0; i < numberOfPbus; ++i) {
      String key = String.format("privacybudgetkey-%d", i);
      builder.add(
          PrivacyBudgetUnit.builder().privacyBudgetKey(key).reportingWindow(testInstant).build());
    }
    ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits = builder.build();
    ReportingOriginToPrivacyBudgetUnits originToBudgetUnitMapping =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setPrivacyBudgetUnits(privacyBudgetUnits)
            .setReportingOrigin("abc.com")
            .build();
    ReportingOriginToPrivacyBudgetUnits originToBudgetUnitMapping2 =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setPrivacyBudgetUnits(privacyBudgetUnits)
            .setReportingOrigin("abc2.com")
            .build();
    if (enableNewClientOptions.equals(EnableNewClientOptions.UNSET)) {
      return ConsumePrivacyBudgetRequest.builder()
          .claimedIdentity("abc.com")
          .trustedServicesClientVersion("dummy-version")
          .reportingOriginToPrivacyBudgetUnitsList(
              ImmutableList.of(originToBudgetUnitMapping, originToBudgetUnitMapping2))
          .build();
    }

    return ConsumePrivacyBudgetRequest.builder()
        .claimedIdentity("abc.com")
        .trustedServicesClientVersion("dummy-version")
        .reportingOriginToPrivacyBudgetUnitsList(ImmutableList.of(originToBudgetUnitMapping))
        .trustedServicesClientVersion("dummy-version")
        .enableNewPbsClient(enableNewClientOptions.equals(EnableNewClientOptions.ON))
        .build();
  }
}
