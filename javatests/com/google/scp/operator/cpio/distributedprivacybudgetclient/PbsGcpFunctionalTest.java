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

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import com.google.acai.Acai;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.ResultSet;
import com.google.cloud.spanner.Statement;
import com.google.common.collect.ImmutableList;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.coordinator.privacy.budgeting.model.ReportingOriginToPrivacyBudgetUnits;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestConfig;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestEnv;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestEnv.Pbs1DatabaseClient;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestEnv.Pbs1Endpoint;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestEnv.Pbs2DatabaseClient;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestEnv.Pbs2Endpoint;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestUtils;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.PrivacyBudgetClient.PrivacyBudgetClientException;
import com.google.scp.shared.api.util.HttpClientWithInterceptor;
import com.google.scp.shared.gcp.util.GcpHttpInterceptorUtil;
import java.sql.Timestamp;
import java.time.Instant;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.testcontainers.containers.Network;

public final class PbsGcpFunctionalTest {
  @Rule public final Acai acai = new Acai(PbsGcpFunctionalTest.TestEnv.class);
  @Inject @Pbs1Endpoint String pbs1Endpoint;
  @Inject @Pbs2Endpoint String pbs2Endpoint;
  @Inject @Pbs1DatabaseClient DatabaseClient pbs1DataClient;
  @Inject @Pbs2DatabaseClient DatabaseClient pbs2DataClient;

  private static final UUID TRANSACTION_ID = UUID.randomUUID();
  private TransactionRequest transactionRequest;

  private PrivacyBudgetClientImpl pbs1PrivacyBudgetClient;
  private PrivacyBudgetClientImpl pbs2PrivacyBudgetClient;

  @Before
  public void setup() {
    this.pbs1PrivacyBudgetClient =
        new PrivacyBudgetClientImpl(
            new HttpClientWithInterceptor(
                GcpHttpInterceptorUtil.createPbsHttpInterceptor(PbsTestUtils.DUMMY_AUTH_ENDPOINT)),
            pbs1Endpoint);
    this.pbs2PrivacyBudgetClient =
        new PrivacyBudgetClientImpl(
            new HttpClientWithInterceptor(
                GcpHttpInterceptorUtil.createPbsHttpInterceptor(PbsTestUtils.DUMMY_AUTH_ENDPOINT)),
            pbs2Endpoint);
    transactionRequest = generateTransactionRequest();
  }

  @Test
  public void performActionBeginSuccess() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.BEGIN, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1PrivacyBudgetClient.performActionBegin(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(
            pbs2Endpoint, TRANSACTION_ID, TransactionPhase.BEGIN, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2PrivacyBudgetClient.performActionBegin(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performActionPrepareSuccess() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1PrivacyBudgetClient.performActionPrepare(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(
            pbs2Endpoint, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2PrivacyBudgetClient.performActionPrepare(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);

    PbsTestConfig pbsTestConfig = PbsTestConfig.builder().build();
    ResultSet pbs1ResultSet =
        pbs1DataClient
            .readOnlyTransaction()
            .executeQuery(Statement.of("SELECT * FROM " + pbsTestConfig.pbs1BudgetKeyTableName()));
    ResultSet pbs2ResultSet =
        pbs2DataClient
            .readOnlyTransaction()
            .executeQuery(Statement.of("SELECT * FROM " + pbsTestConfig.pbs2BudgetKeyTableName()));

    final Set<String> budgetKeys =
        new HashSet<>(
            Arrays.asList(
                "dummy-reporting-origin.com/budgetkey1", "dummy-reporting-origin.com/budgetkey2"));
    Set<String> pbs1BudgetKeysActual = new HashSet<>();
    Set<String> pbs2BudgetKeysActual = new HashSet<>();
    final String timeFrame = "19";
    final String valueJson = "{\"TokenCount\":\"1 1 1 1 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1\"}";

    int pbs1ResultSize = 0;
    while (pbs1ResultSet.next()) {
      pbs1BudgetKeysActual.add(pbs1ResultSet.getString(0));
      assertEquals(pbs1ResultSet.getString(1), timeFrame);
      assertEquals(pbs1ResultSet.getJson(2), valueJson);
      pbs1ResultSize += 1;
    }
    assertTrue(budgetKeys.containsAll(pbs1BudgetKeysActual));
    assertEquals(pbs1ResultSize, 2);

    int pbs2ResultSize = 0;
    while (pbs2ResultSet.next()) {
      pbs2BudgetKeysActual.add(pbs2ResultSet.getString(0));
      assertEquals(pbs2ResultSet.getString(1), timeFrame);
      assertEquals(pbs2ResultSet.getJson(2), valueJson);
      pbs2ResultSize += 1;
    }
    assertTrue(budgetKeys.containsAll(pbs2BudgetKeysActual));
    assertEquals(pbs2ResultSize, 2);

    deleteAllRowsFromPbsTables(pbsTestConfig);
  }

  @Test
  public void performActionPrepareBudgetExhausted() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1PrivacyBudgetClient.performActionPrepare(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(
            pbs2Endpoint, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2PrivacyBudgetClient.performActionPrepare(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertTrue(pbs1Transaction.getExhaustedPrivacyBudgetUnits().isEmpty());
    assertTrue(pbs2Transaction.getExhaustedPrivacyBudgetUnits().isEmpty());

    ExecutionResult pbs1ExecutionResultBudgetExhausted =
        pbs1PrivacyBudgetClient.performActionPrepare(pbs1Transaction);
    ExecutionResult pbs2ExecutionResultBudgetExhausted =
        pbs2PrivacyBudgetClient.performActionPrepare(pbs2Transaction);

    assertThat(pbs1ExecutionResultBudgetExhausted.executionStatus())
        .isEqualTo(ExecutionStatus.FAILURE);
    assertThat(pbs1ExecutionResultBudgetExhausted.statusCode()).isEqualTo(StatusCode.UNKNOWN);
    assertThat(pbs2ExecutionResultBudgetExhausted.executionStatus())
        .isEqualTo(ExecutionStatus.FAILURE);
    assertThat(pbs2ExecutionResultBudgetExhausted.statusCode()).isEqualTo(StatusCode.UNKNOWN);

    final PrivacyBudgetUnit privacyBudgetUnit1 =
        PrivacyBudgetUnit.builder()
            .privacyBudgetKey("budgetkey1")
            .reportingWindow(Instant.parse("1970-01-20T04:49:20.799Z"))
            .build();
    final PrivacyBudgetUnit privacyBudgetUnit2 =
        PrivacyBudgetUnit.builder()
            .privacyBudgetKey("budgetkey2")
            .reportingWindow(Instant.parse("1970-01-20T04:49:20.845Z"))
            .build();
    final ReportingOriginToPrivacyBudgetUnits exhaustedBudgetUnitsByOrigin =
        ReportingOriginToPrivacyBudgetUnits.builder()
            .setReportingOrigin("dummy-reporting-origin.com")
            .setPrivacyBudgetUnits(ImmutableList.of(privacyBudgetUnit1, privacyBudgetUnit2))
            .build();
    assertThat(pbs1Transaction.getExhaustedPrivacyBudgetUnits())
        .contains(exhaustedBudgetUnitsByOrigin);
    assertThat(pbs2Transaction.getExhaustedPrivacyBudgetUnits())
        .contains(exhaustedBudgetUnitsByOrigin);

    PbsTestConfig pbsTestConfig = PbsTestConfig.builder().build();
    deleteAllRowsFromPbsTables(pbsTestConfig);
  }

  @Test
  public void performActionCommitSuccess() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.COMMIT, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1PrivacyBudgetClient.performActionCommit(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.COMMIT, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs1PrivacyBudgetClient.performActionCommit(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performActionNotifySuccess() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.NOTIFY, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1PrivacyBudgetClient.performActionNotify(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.NOTIFY, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs1PrivacyBudgetClient.performActionNotify(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performActionAbortSuccess() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.ABORT, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1PrivacyBudgetClient.performActionAbort(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.ABORT, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs1PrivacyBudgetClient.performActionAbort(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performActionEndSuccess() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(pbs1Endpoint, TRANSACTION_ID, TransactionPhase.END, transactionRequest);
    ExecutionResult pbs1ExecutionResult = pbs1PrivacyBudgetClient.performActionEnd(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(pbs1Endpoint, TRANSACTION_ID, TransactionPhase.END, transactionRequest);
    ExecutionResult pbs2ExecutionResult = pbs1PrivacyBudgetClient.performActionEnd(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  private void deleteAllRowsFromPbsTables(PbsTestConfig pbsTestConfig) {
    pbs1DataClient
        .readWriteTransaction()
        .run(
            tx -> {
              tx.executeUpdate(
                  Statement.of(
                      "DELETE FROM " + pbsTestConfig.pbs1BudgetKeyTableName() + " WHERE true"));
              return null;
            });
    pbs2DataClient
        .readWriteTransaction()
        .run(
            tx -> {
              tx.executeUpdate(
                  Statement.of(
                      "DELETE FROM " + pbsTestConfig.pbs2BudgetKeyTableName() + " WHERE true"));
              return null;
            });
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
        .setReportingOriginToPrivacyBudgetUnitsList(
            ImmutableList.of(
                ReportingOriginToPrivacyBudgetUnits.builder()
                    .setPrivacyBudgetUnits(ImmutableList.of(unit1, unit2))
                    .setReportingOrigin("dummy-reporting-origin.com")
                    .build()))
        .setTransactionSecret("transaction-secret")
        .setTimeout(Timestamp.from(Instant.now()))
        .setClaimedIdentity("https://dummy-reporting-origin.com")
        .build();
  }

  private static class TestEnv extends AbstractModule {

    @Override
    public void configure() {
      var network = Network.newNetwork();
      install(new PbsTestEnv(PbsTestConfig.builder().build(), network));
    }
  }
}
