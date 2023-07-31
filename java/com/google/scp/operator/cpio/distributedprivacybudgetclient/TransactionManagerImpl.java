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

import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionEngine.TransactionEngineException;

/**
 * Transaction manager is responsible for coordinating with 1 or more coordinators to run 2-phase
 * commit transactions.
 */
public class TransactionManagerImpl implements TransactionManager {
  private final TransactionEngine transactionEngine;

  /** Creates a new instance of the {@code TransactionManagerImpl} class. */
  @Inject
  public TransactionManagerImpl(TransactionEngine transactionEngine) {
    this.transactionEngine = transactionEngine;
  }

  /**
   * @param request Transaction request metadata.
   */
  @Override
  public ImmutableList<PrivacyBudgetUnit> execute(TransactionRequest request)
      throws TransactionManagerException {
    try {
      return transactionEngine.execute(request);
    } catch (TransactionEngineException e) {
      throw new TransactionManagerException(e.getStatusCode(), e);
    }
  }

  /**
   * @param transactionPhaseRequest Transaction phase request metadata.
   */
  @Override
  public void executePhase(TransactionPhaseRequest transactionPhaseRequest)
      throws TransactionManagerException {}
}
