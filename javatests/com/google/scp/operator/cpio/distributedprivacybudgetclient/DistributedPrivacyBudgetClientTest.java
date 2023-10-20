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
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClient.DistributedPrivacyBudgetClientException;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClient.DistributedPrivacyBudgetServiceException;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionEngine.TransactionEngineException;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionManager.TransactionManagerException;
import java.time.Instant;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class DistributedPrivacyBudgetClientTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Mock private TransactionManagerImpl transactionManager;
  private DistributedPrivacyBudgetClientImpl distributedPrivacyBudgetClient;

  Instant testInstant = Instant.now();

  @Before
  public void setUp() {
    distributedPrivacyBudgetClient = new DistributedPrivacyBudgetClientImpl(transactionManager);
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
    when(transactionManager.execute(any(TransactionRequest.class))).thenReturn(exhaustedUnits);

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        distributedPrivacyBudgetClient.consumePrivacyBudget(getRequest(1));

    verify(transactionManager).execute(argument.capture());
    assertThat(consumePrivacyBudgetResponse).isEqualTo(getExhaustedBudget());
  }

  @Test
  public void consumePrivacyBudget_nonExhaustedBudget() throws Exception {
    var argument = ArgumentCaptor.forClass(TransactionRequest.class);
    when(transactionManager.execute(any(TransactionRequest.class)))
        .thenReturn(ImmutableList.<PrivacyBudgetUnit>builder().build());

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        distributedPrivacyBudgetClient.consumePrivacyBudget(getRequest(1));

    verify(transactionManager).execute(argument.capture());
    assertThat(consumePrivacyBudgetResponse).isEqualTo(getNotExhaustedBudget());
  }

  @Test
  public void consumePrivacyBudget_overSizeLimit() {
    assertThrows(
        DistributedPrivacyBudgetClientException.class,
        () -> distributedPrivacyBudgetClient.consumePrivacyBudget(getRequest(30001)));
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
            () -> distributedPrivacyBudgetClient.consumePrivacyBudget(getRequest(1)));
    verify(transactionManager).execute(argument.capture());
    assertThat(exception.getMessage()).isEqualTo(StatusCode.UNKNOWN.name());
    assertThat(exception.getCause().getCause().getMessage())
        .isEqualTo(StatusCode.PRIVACY_BUDGET_CLIENT_UNAUTHENTICATED.name());
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
    return ConsumePrivacyBudgetResponse.builder()
        .exhaustedPrivacyBudgetUnits(privacyBudgetUnits)
        .build();
  }

  private ConsumePrivacyBudgetResponse getNotExhaustedBudget() {
    return ConsumePrivacyBudgetResponse.builder()
        .exhaustedPrivacyBudgetUnits(ImmutableList.<PrivacyBudgetUnit>builder().build())
        .build();
  }

  private ConsumePrivacyBudgetRequest getRequest(int count) {
    var builder = ImmutableList.<PrivacyBudgetUnit>builder();
    for (int i = 0; i < count; ++i) {
      var key = String.format("privacybudgetkey-%d", i);
      builder.add(
          PrivacyBudgetUnit.builder().privacyBudgetKey(key).reportingWindow(testInstant).build());
    }
    var privacyBudgetUnits = builder.build();
    return ConsumePrivacyBudgetRequest.builder()
        .attributionReportTo("abc.com")
        .privacyBudgetUnits(privacyBudgetUnits)
        .build();
  }
}
