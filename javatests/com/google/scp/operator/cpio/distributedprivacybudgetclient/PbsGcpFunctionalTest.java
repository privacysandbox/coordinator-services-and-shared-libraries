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
import static org.junit.Assert.assertTrue;

import com.google.acai.Acai;
import com.google.auto.value.AutoValue;
import com.google.cloud.spanner.DatabaseClient;
import com.google.cloud.spanner.ResultSet;
import com.google.cloud.spanner.Statement;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import com.google.common.truth.Expect;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.coordinator.privacy.budgeting.model.ReportingOriginToPrivacyBudgetUnits;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestConfig;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestEnv;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestEnv.Pbs1DatabaseClient;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestEnv.Pbs1Endpoint;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestEnv.Pbs2DatabaseClient;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestEnv.Pbs2Endpoint;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestEnv.PbsAuthEndpoint;
import com.google.scp.coordinator.privacy.budgeting.utils.gcp.PbsTestUtils;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClient.DistributedPrivacyBudgetClientException;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClient.DistributedPrivacyBudgetServiceException;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.PrivacyBudgetClient.PrivacyBudgetClientException;
import com.google.scp.shared.api.util.HttpClientWithInterceptor;
import com.google.scp.shared.aws.util.AwsAuthTokenInterceptor;
import com.google.scp.shared.gcp.util.GcpHttpInterceptorUtil;
import com.google.testing.junit.testparameterinjector.TestParameter;
import com.google.testing.junit.testparameterinjector.TestParameterInjector;
import java.sql.Timestamp;
import java.time.Instant;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.testcontainers.containers.Network;
import software.amazon.awssdk.auth.credentials.AwsBasicCredentials;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.StaticCredentialsProvider;
import software.amazon.awssdk.regions.Region;

@RunWith(TestParameterInjector.class)
public final class PbsGcpFunctionalTest {
  @Rule public final Acai acai = new Acai(PbsGcpFunctionalTest.TestEnv.class);
  @Rule public final Expect expect = Expect.create();
  @Inject @Pbs1Endpoint String pbs1Endpoint;
  @Inject @Pbs2Endpoint String pbs2Endpoint;
  @Inject @PbsAuthEndpoint String pbsAuthEndpoint;
  @Inject @Pbs1DatabaseClient DatabaseClient pbs1DataClient;
  @Inject @Pbs2DatabaseClient DatabaseClient pbs2DataClient;

  private static final UUID TRANSACTION_ID = UUID.randomUUID();
  private TransactionRequest transactionRequest;
  private PbsTestConfig pbsTestConfig;

  private PrivacyBudgetClientImpl pbs1PrivacyBudgetClient;
  private PrivacyBudgetClientImpl pbs2PrivacyBudgetClient;

  private PrivacyBudgetClientImpl pbs1AwsPrivacyBudgetClient;
  private PrivacyBudgetClientImpl pbs2AwsPrivacyBudgetClient;
  private PrivacyBudgetClientImplV2 pbs1PrivacyBudgetClientV2;
  private PrivacyBudgetClientImplV2 pbs2PrivacyBudgetClientV2;

  private DistributedPrivacyBudgetClient distributedPrivacyBudgetClientGcp;
  private DistributedPrivacyBudgetClient distributedPrivacyBudgetClientAws;

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

    AwsCredentialsProvider credsProvider =
        StaticCredentialsProvider.create(AwsBasicCredentials.create("hello", "world"));
    this.pbs1AwsPrivacyBudgetClient =
        new PrivacyBudgetClientImpl(
            new HttpClientWithInterceptor(
                new AwsAuthTokenInterceptor(
                    Region.US_WEST_1, PbsTestUtils.DUMMY_AUTH_ENDPOINT, credsProvider)),
            pbs1Endpoint);
    this.pbs2AwsPrivacyBudgetClient =
        new PrivacyBudgetClientImpl(
            new HttpClientWithInterceptor(
                new AwsAuthTokenInterceptor(
                    Region.US_WEST_1, PbsTestUtils.DUMMY_AUTH_ENDPOINT, credsProvider)),
            pbs2Endpoint);

    this.pbs1PrivacyBudgetClientV2 =
        new PrivacyBudgetClientImplV2(
            new HttpClientWithInterceptor(
                GcpHttpInterceptorUtil.createPbsHttpInterceptor(PbsTestUtils.DUMMY_AUTH_ENDPOINT)),
            pbs1Endpoint);
    this.pbs2PrivacyBudgetClientV2 =
        new PrivacyBudgetClientImplV2(
            new HttpClientWithInterceptor(
                GcpHttpInterceptorUtil.createPbsHttpInterceptor(PbsTestUtils.DUMMY_AUTH_ENDPOINT)),
            pbs2Endpoint);
    this.distributedPrivacyBudgetClientGcp =
        PbsTestUtils.createDistributedPbsClient(pbs1Endpoint, pbs2Endpoint, pbsAuthEndpoint);
    this.distributedPrivacyBudgetClientAws =
        createDistributedPbsClientAws(
            credsProvider, Region.US_WEST_1, pbs1Endpoint, pbs2Endpoint, pbsAuthEndpoint);

    this.transactionRequest = generateTransactionRequest();
    this.pbsTestConfig = PbsTestConfig.builder().build();
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
  public void performActionBeginFromAwsCrossCloudSuccess() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.BEGIN, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1AwsPrivacyBudgetClient.performActionBegin(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(
            pbs2Endpoint, TRANSACTION_ID, TransactionPhase.BEGIN, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2AwsPrivacyBudgetClient.performActionBegin(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performActionHealthCheckSuccess() {
    HealthCheckResponse pbs1HealthCheckResponse =
        pbs1PrivacyBudgetClientV2.performActionHealthCheck(transactionRequest);

    HealthCheckResponse pbs2HealthCheckResponse =
        pbs2PrivacyBudgetClientV2.performActionHealthCheck(transactionRequest);

    expect
        .that(pbs1HealthCheckResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.SUCCESS);
    expect.that(pbs1HealthCheckResponse.executionResult().statusCode()).isEqualTo(StatusCode.OK);
    expect
        .that(pbs2HealthCheckResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.SUCCESS);
    expect.that(pbs2HealthCheckResponse.executionResult().statusCode()).isEqualTo(StatusCode.OK);
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

    ResultSet pbs1ResultSet =
        pbs1DataClient
            .readOnlyTransaction()
            .executeQuery(
                Statement.of("SELECT * FROM " + this.pbsTestConfig.pbs1BudgetKeyTableName()));
    ResultSet pbs2ResultSet =
        pbs2DataClient
            .readOnlyTransaction()
            .executeQuery(
                Statement.of("SELECT * FROM " + this.pbsTestConfig.pbs2BudgetKeyTableName()));

    ImmutableSet<BudgetUnitSchema> expectedBudgetUnits = createExpectedBudgetUnits();
    ImmutableSet<BudgetUnitSchema> pbs1ActualUnits =
        extractBudgetUnitSchemasFromResultSet(pbs1ResultSet);
    ImmutableSet<BudgetUnitSchema> pbs2ActualUnits =
        extractBudgetUnitSchemasFromResultSet(pbs2ResultSet);

    expect.that(pbs1ActualUnits).containsExactlyElementsIn(expectedBudgetUnits);
    expect.that(pbs2ActualUnits).containsExactlyElementsIn(expectedBudgetUnits);

    deleteAllRowsFromPbsTables();
  }

  @Test
  public void performActionPrepareFromAwsCrossCloudSuccess() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1AwsPrivacyBudgetClient.performActionPrepare(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(
            pbs2Endpoint, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2AwsPrivacyBudgetClient.performActionPrepare(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);

    ResultSet pbs1ResultSet =
        pbs1DataClient
            .readOnlyTransaction()
            .executeQuery(
                Statement.of("SELECT * FROM " + this.pbsTestConfig.pbs1BudgetKeyTableName()));
    ResultSet pbs2ResultSet =
        pbs2DataClient
            .readOnlyTransaction()
            .executeQuery(
                Statement.of("SELECT * FROM " + this.pbsTestConfig.pbs2BudgetKeyTableName()));

    ImmutableSet<BudgetUnitSchema> expectedBudgetUnits = createExpectedBudgetUnits();
    ImmutableSet<BudgetUnitSchema> pbs1ActualUnits =
        extractBudgetUnitSchemasFromResultSet(pbs1ResultSet);
    ImmutableSet<BudgetUnitSchema> pbs2ActualUnits =
        extractBudgetUnitSchemasFromResultSet(pbs2ResultSet);

    expect.that(pbs1ActualUnits).containsExactlyElementsIn(expectedBudgetUnits);
    expect.that(pbs2ActualUnits).containsExactlyElementsIn(expectedBudgetUnits);

    deleteAllRowsFromPbsTables();
  }

  @Test
  public void performActionConsumeBudgetSuccess() {
    ConsumeBudgetResponse pbs1ConsumeBudgetResponse =
        pbs1PrivacyBudgetClientV2.performActionConsumeBudget(transactionRequest);

    ConsumeBudgetResponse pbs2ConsumeBudgetResponse =
        pbs2PrivacyBudgetClientV2.performActionConsumeBudget(transactionRequest);

    assertThat(pbs1ConsumeBudgetResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ConsumeBudgetResponse.executionResult().statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ConsumeBudgetResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ConsumeBudgetResponse.executionResult().statusCode()).isEqualTo(StatusCode.OK);

    ResultSet pbs1ResultSet =
        pbs1DataClient
            .readOnlyTransaction()
            .executeQuery(
                Statement.of("SELECT * FROM " + this.pbsTestConfig.pbs1BudgetKeyTableName()));
    ResultSet pbs2ResultSet =
        pbs2DataClient
            .readOnlyTransaction()
            .executeQuery(
                Statement.of("SELECT * FROM " + this.pbsTestConfig.pbs2BudgetKeyTableName()));

    ImmutableSet<BudgetUnitSchema> expectedBudgetUnits = createExpectedBudgetUnits();
    ImmutableSet<BudgetUnitSchema> pbs1ActualUnits =
        extractBudgetUnitSchemasFromResultSet(pbs1ResultSet);
    ImmutableSet<BudgetUnitSchema> pbs2ActualUnits =
        extractBudgetUnitSchemasFromResultSet(pbs2ResultSet);

    expect.that(pbs1ActualUnits).containsExactlyElementsIn(expectedBudgetUnits);
    expect.that(pbs2ActualUnits).containsExactlyElementsIn(expectedBudgetUnits);

    deleteAllRowsFromPbsTables();
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

    deleteAllRowsFromPbsTables();
  }

  @Test
  public void performActionPrepareFromAwsCrossCloudBudgetExhausted()
      throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1AwsPrivacyBudgetClient.performActionPrepare(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(
            pbs2Endpoint, TRANSACTION_ID, TransactionPhase.PREPARE, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2AwsPrivacyBudgetClient.performActionPrepare(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertTrue(pbs1Transaction.getExhaustedPrivacyBudgetUnits().isEmpty());
    assertTrue(pbs2Transaction.getExhaustedPrivacyBudgetUnits().isEmpty());

    ExecutionResult pbs1ExecutionResultBudgetExhausted =
        pbs1AwsPrivacyBudgetClient.performActionPrepare(pbs1Transaction);
    ExecutionResult pbs2ExecutionResultBudgetExhausted =
        pbs2AwsPrivacyBudgetClient.performActionPrepare(pbs2Transaction);

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

    deleteAllRowsFromPbsTables();
  }

  @Test
  public void performActionConsumeBudgetBudgetExhausted() {
    ConsumeBudgetResponse pbs1ConsumeBudgetResponse =
        pbs1PrivacyBudgetClientV2.performActionConsumeBudget(transactionRequest);

    ConsumeBudgetResponse pbs2ConsumeBudgetResponse =
        pbs2PrivacyBudgetClientV2.performActionConsumeBudget(transactionRequest);

    assertThat(pbs1ConsumeBudgetResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ConsumeBudgetResponse.executionResult().statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ConsumeBudgetResponse.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ConsumeBudgetResponse.executionResult().statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs1ConsumeBudgetResponse.exhaustedPrivacyBudgetUnitsByOrigin()).isEmpty();
    assertThat(pbs2ConsumeBudgetResponse.exhaustedPrivacyBudgetUnitsByOrigin()).isEmpty();

    ConsumeBudgetResponse pbs1ConsumeBudgetResponseBudgetExhausted =
        pbs1PrivacyBudgetClientV2.performActionConsumeBudget(transactionRequest);
    ConsumeBudgetResponse pbs2ConsumeBudgetResponseBudgetExhausted =
        pbs2PrivacyBudgetClientV2.performActionConsumeBudget(transactionRequest);

    expect
        .that(pbs1ConsumeBudgetResponseBudgetExhausted.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.FAILURE);
    expect
        .that(pbs1ConsumeBudgetResponseBudgetExhausted.executionResult().statusCode())
        .isEqualTo(StatusCode.PRIVACY_BUDGET_CLIENT_BUDGET_EXHAUSTED);
    expect
        .that(pbs2ConsumeBudgetResponseBudgetExhausted.executionResult().executionStatus())
        .isEqualTo(ExecutionStatus.FAILURE);
    expect
        .that(pbs2ConsumeBudgetResponseBudgetExhausted.executionResult().statusCode())
        .isEqualTo(StatusCode.PRIVACY_BUDGET_CLIENT_BUDGET_EXHAUSTED);

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
    expect
        .that(pbs1ConsumeBudgetResponseBudgetExhausted.exhaustedPrivacyBudgetUnitsByOrigin())
        .contains(exhaustedBudgetUnitsByOrigin);
    expect
        .that(pbs2ConsumeBudgetResponseBudgetExhausted.exhaustedPrivacyBudgetUnitsByOrigin())
        .contains(exhaustedBudgetUnitsByOrigin);

    deleteAllRowsFromPbsTables();
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
            pbs2Endpoint, TRANSACTION_ID, TransactionPhase.COMMIT, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2PrivacyBudgetClient.performActionCommit(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performActionCommitFromAwsCrossCloudSuccess() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.COMMIT, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1AwsPrivacyBudgetClient.performActionCommit(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(
            pbs2Endpoint, TRANSACTION_ID, TransactionPhase.COMMIT, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2AwsPrivacyBudgetClient.performActionCommit(pbs2Transaction);

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
            pbs2Endpoint, TRANSACTION_ID, TransactionPhase.NOTIFY, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2PrivacyBudgetClient.performActionNotify(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performActionNotifyFromAwsCrossCloudSuccess() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.NOTIFY, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1AwsPrivacyBudgetClient.performActionNotify(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(
            pbs2Endpoint, TRANSACTION_ID, TransactionPhase.NOTIFY, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2AwsPrivacyBudgetClient.performActionNotify(pbs2Transaction);

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
            pbs2Endpoint, TRANSACTION_ID, TransactionPhase.ABORT, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2PrivacyBudgetClient.performActionAbort(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performActionAbortFromAwsCrossCloudSuccess() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(
            pbs1Endpoint, TRANSACTION_ID, TransactionPhase.ABORT, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1AwsPrivacyBudgetClient.performActionAbort(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(
            pbs2Endpoint, TRANSACTION_ID, TransactionPhase.ABORT, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2AwsPrivacyBudgetClient.performActionAbort(pbs2Transaction);

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
        generateTransaction(pbs2Endpoint, TRANSACTION_ID, TransactionPhase.END, transactionRequest);
    ExecutionResult pbs2ExecutionResult = pbs2PrivacyBudgetClient.performActionEnd(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void performActionEndFromAwsCrossCloudSuccess() throws PrivacyBudgetClientException {
    Transaction pbs1Transaction =
        generateTransaction(pbs1Endpoint, TRANSACTION_ID, TransactionPhase.END, transactionRequest);
    ExecutionResult pbs1ExecutionResult =
        pbs1AwsPrivacyBudgetClient.performActionEnd(pbs1Transaction);

    Transaction pbs2Transaction =
        generateTransaction(pbs2Endpoint, TRANSACTION_ID, TransactionPhase.END, transactionRequest);
    ExecutionResult pbs2ExecutionResult =
        pbs2AwsPrivacyBudgetClient.performActionEnd(pbs2Transaction);

    assertThat(pbs1ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs1ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
    assertThat(pbs2ExecutionResult.executionStatus()).isEqualTo(ExecutionStatus.SUCCESS);
    assertThat(pbs2ExecutionResult.statusCode()).isEqualTo(StatusCode.OK);
  }

  @Test
  public void testFromDistributedClientGcp(@TestParameter boolean enableNewClientFlag)
      throws DistributedPrivacyBudgetServiceException, DistributedPrivacyBudgetClientException {
    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        this.distributedPrivacyBudgetClientGcp.consumePrivacyBudget(
            createConsumePrivacyBudgetRequest(enableNewClientFlag));
    expect.that(consumePrivacyBudgetResponse.exhaustedPrivacyBudgetUnitsByOrigin()).isEmpty();

    deleteAllRowsFromPbsTables();
  }

  @Test
  public void testFromDistributedClientBudgetExhaustedGcp(
      @TestParameter boolean enableNewClientFlag)
      throws DistributedPrivacyBudgetServiceException, DistributedPrivacyBudgetClientException {
    ConsumePrivacyBudgetRequest consumePrivacyBudgetRequest =
        createConsumePrivacyBudgetRequest(enableNewClientFlag);

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        this.distributedPrivacyBudgetClientGcp.consumePrivacyBudget(consumePrivacyBudgetRequest);
    assertThat(consumePrivacyBudgetResponse.exhaustedPrivacyBudgetUnitsByOrigin()).isEmpty();

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponseBudgetExhausted =
        this.distributedPrivacyBudgetClientGcp.consumePrivacyBudget(consumePrivacyBudgetRequest);
    expect
        .that(consumePrivacyBudgetResponseBudgetExhausted.exhaustedPrivacyBudgetUnitsByOrigin())
        .containsExactlyElementsIn(
            consumePrivacyBudgetRequest.reportingOriginToPrivacyBudgetUnitsList());

    deleteAllRowsFromPbsTables();
  }

  @Test
  public void testFromDistributedClientAws(@TestParameter boolean enableNewClientFlag)
      throws DistributedPrivacyBudgetServiceException, DistributedPrivacyBudgetClientException {
    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        this.distributedPrivacyBudgetClientAws.consumePrivacyBudget(
            createConsumePrivacyBudgetRequest(enableNewClientFlag));
    expect.that(consumePrivacyBudgetResponse.exhaustedPrivacyBudgetUnitsByOrigin()).isEmpty();
    deleteAllRowsFromPbsTables();
  }

  @Test
  public void testFromDistributedClientBudgetExhaustedAws(
      @TestParameter boolean enableNewClientFlag)
      throws DistributedPrivacyBudgetServiceException, DistributedPrivacyBudgetClientException {
    ConsumePrivacyBudgetRequest consumePrivacyBudgetRequest =
        createConsumePrivacyBudgetRequest(enableNewClientFlag);

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        this.distributedPrivacyBudgetClientAws.consumePrivacyBudget(consumePrivacyBudgetRequest);
    assertThat(consumePrivacyBudgetResponse.exhaustedPrivacyBudgetUnitsByOrigin()).isEmpty();

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponseBudgetExhausted =
        this.distributedPrivacyBudgetClientAws.consumePrivacyBudget(consumePrivacyBudgetRequest);
    expect
        .that(consumePrivacyBudgetResponseBudgetExhausted.exhaustedPrivacyBudgetUnitsByOrigin())
        .containsExactlyElementsIn(
            consumePrivacyBudgetRequest.reportingOriginToPrivacyBudgetUnitsList());

    deleteAllRowsFromPbsTables();
  }

  private void deleteAllRowsFromPbsTables() {
    pbs1DataClient
        .readWriteTransaction()
        .run(
            tx -> {
              tx.executeUpdate(
                  Statement.of(
                      "DELETE FROM "
                          + this.pbsTestConfig.pbs1BudgetKeyTableName()
                          + " WHERE true"));
              return null;
            });
    pbs2DataClient
        .readWriteTransaction()
        .run(
            tx -> {
              tx.executeUpdate(
                  Statement.of(
                      "DELETE FROM "
                          + this.pbsTestConfig.pbs2BudgetKeyTableName()
                          + " WHERE true"));
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
    if (currentPhase != TransactionPhase.BEGIN && currentPhase != TransactionPhase.HEALTH_CHECK) {
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
        .setTrustedServicesClientVersion("dummy-version")
        .build();
  }

  private ConsumePrivacyBudgetRequest createConsumePrivacyBudgetRequest(boolean enableNewClient) {
    final PrivacyBudgetUnit privacyBudgetUnit1 =
        PrivacyBudgetUnit.builder()
            .privacyBudgetKey("budgetkey1")
            .reportingWindow(Instant.ofEpochMilli(1658960799))
            .build();
    final PrivacyBudgetUnit privacyBudgetUnit2 =
        PrivacyBudgetUnit.builder()
            .privacyBudgetKey("budgetkey2")
            .reportingWindow(Instant.ofEpochMilli(1658960845))
            .build();
    final PrivacyBudgetUnit privacyBudgetUnit3 =
        PrivacyBudgetUnit.builder()
            .privacyBudgetKey("budgetkey3")
            .reportingWindow(Instant.ofEpochMilli(1658960891))
            .build();
    return ConsumePrivacyBudgetRequest.builder()
        .reportingOriginToPrivacyBudgetUnitsList(
            ImmutableList.of(
                ReportingOriginToPrivacyBudgetUnits.builder()
                    .setPrivacyBudgetUnits(
                        ImmutableList.of(
                            privacyBudgetUnit1, privacyBudgetUnit2, privacyBudgetUnit3))
                    .setReportingOrigin("dummy-reporting-origin.com")
                    .build()))
        .enableNewPbsClient(enableNewClient)
        .trustedServicesClientVersion("dummy-version")
        .claimedIdentity("https://dummy-reporting-origin.com")
        .build();
  }

  public static DistributedPrivacyBudgetClient createDistributedPbsClientAws(
      AwsCredentialsProvider awsCredentialsProvider,
      Region region,
      String pbs1Endpoint,
      String pbs2Endpoint,
      String authEndpoint) {
    AwsAuthTokenInterceptor awsAuthTokenInterceptor =
        new AwsAuthTokenInterceptor(region, authEndpoint, awsCredentialsProvider);
    HttpClientWithInterceptor httpClient = new HttpClientWithInterceptor(awsAuthTokenInterceptor);
    PrivacyBudgetClient pbs1Client = new PrivacyBudgetClientImpl(httpClient, pbs1Endpoint);
    PrivacyBudgetClient pbs2Client = new PrivacyBudgetClientImpl(httpClient, pbs2Endpoint);
    TransactionEngine transactionEngine =
        new TransactionEngineImpl(
            new TransactionPhaseManagerImpl(), ImmutableList.of(pbs1Client, pbs2Client));
    TransactionManager transactionManager = new TransactionManagerImpl(transactionEngine);

    PrivacyBudgetClientV2 pbsV2Client1 = new PrivacyBudgetClientImplV2(httpClient, pbs1Endpoint);
    PrivacyBudgetClientV2 pbsV2Client2 = new PrivacyBudgetClientImplV2(httpClient, pbs2Endpoint);
    TransactionOrchestrator transactionOrchestrator =
        new TransactionOrchestratorImpl(ImmutableList.of(pbsV2Client1, pbsV2Client2));
    return new DistributedPrivacyBudgetClientImpl(transactionManager, transactionOrchestrator);
  }

  private static ImmutableSet<BudgetUnitSchema> createExpectedBudgetUnits() {
    return ImmutableSet.of(
        BudgetUnitSchema.Builder()
            .key("dummy-reporting-origin.com/budgetkey1")
            .timeFrame("19")
            .valueJson("{\"TokenCount\":\"1 1 1 1 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1\"}")
            .build(),
        BudgetUnitSchema.Builder()
            .key("dummy-reporting-origin.com/budgetkey2")
            .timeFrame("19")
            .valueJson("{\"TokenCount\":\"1 1 1 1 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1\"}")
            .build());
  }

  private static ImmutableSet<BudgetUnitSchema> extractBudgetUnitSchemasFromResultSet(
      ResultSet resultSet) {
    ImmutableSet.Builder<BudgetUnitSchema> builder = ImmutableSet.builder();
    while (resultSet.next()) {
      builder.add(
          BudgetUnitSchema.Builder()
              .key(resultSet.getString(0))
              .timeFrame(resultSet.getString(1))
              .valueJson(resultSet.getJson(2))
              .build());
    }
    return builder.build();
  }

  @AutoValue
  abstract static class BudgetUnitSchema {
    abstract String key();

    abstract String timeFrame();

    abstract String valueJson();

    static Builder Builder() {
      return new AutoValue_PbsGcpFunctionalTest_BudgetUnitSchema.Builder();
    }

    @AutoValue.Builder
    abstract static class Builder {
      abstract Builder key(String key);

      abstract Builder timeFrame(String timeFrame);

      abstract Builder valueJson(String valueJson);

      abstract BudgetUnitSchema build();
    }
  }

  private static class TestEnv extends AbstractModule {

    @Override
    public void configure() {
      var network = Network.newNetwork();
      install(new PbsTestEnv(PbsTestConfig.builder().build(), network));
    }
  }
}
