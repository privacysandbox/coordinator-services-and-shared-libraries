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

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class TransactionPhaseManagerTest {

  private TransactionPhaseManagerImpl transactionPhaseManager = new TransactionPhaseManagerImpl();

  @Test
  public void proceedToNextPhase_UNKNOWNPhase() {
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.UNKNOWN,
                ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK)))
        .isEqualTo(TransactionPhase.ABORT);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.UNKNOWN,
                ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.ABORT);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.UNKNOWN,
                ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.ABORT);
  }

  @Test
  public void proceedToNextPhase_NOTSTARTEDPhase() {
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.NOTSTARTED,
                ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK)))
        .isEqualTo(TransactionPhase.BEGIN);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.NOTSTARTED,
                ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.END);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.NOTSTARTED,
                ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.NOTSTARTED);
  }

  @Test
  public void proceedToNextPhase_BEGINPhase() {
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.BEGIN,
                ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK)))
        .isEqualTo(TransactionPhase.PREPARE);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.BEGIN,
                ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.ABORT);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.BEGIN,
                ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.BEGIN);
  }

  @Test
  public void proceedToNextPhase_PREPAREPhase() {
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.PREPARE,
                ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK)))
        .isEqualTo(TransactionPhase.COMMIT);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.PREPARE,
                ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.END);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.PREPARE,
                ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.PREPARE);
  }

  @Test
  public void proceedToNextPhase_COMMITPhase() {
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.COMMIT,
                ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK)))
        .isEqualTo(TransactionPhase.NOTIFY);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.COMMIT,
                ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.ABORT);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.COMMIT,
                ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.COMMIT);
  }

  @Test
  public void proceedToNextPhase_NOTIFYPhase() {
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.NOTIFY,
                ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK)))
        .isEqualTo(TransactionPhase.END);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.NOTIFY,
                ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.NOTIFY);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.NOTIFY,
                ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.NOTIFY);
  }

  @Test
  public void proceedToNextPhase_ABORTPhase() {
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.ABORT,
                ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK)))
        .isEqualTo(TransactionPhase.END);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.ABORT,
                ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.ABORT);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.ABORT,
                ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.ABORT);
  }

  @Test
  public void proceedToNextPhase_ENDPhase() {
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.END,
                ExecutionResult.create(ExecutionStatus.SUCCESS, StatusCode.OK)))
        .isEqualTo(TransactionPhase.FINISHED);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.END,
                ExecutionResult.create(ExecutionStatus.FAILURE, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.FINISHED);
    assertThat(
            transactionPhaseManager.proceedToNextPhase(
                TransactionPhase.END,
                ExecutionResult.create(ExecutionStatus.RETRY, StatusCode.UNKNOWN)))
        .isEqualTo(TransactionPhase.END);
  }
}
