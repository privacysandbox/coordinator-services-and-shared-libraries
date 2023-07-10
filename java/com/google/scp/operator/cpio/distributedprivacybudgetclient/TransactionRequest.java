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

import com.google.auto.value.AutoValue;
import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import java.sql.Timestamp;
import java.util.UUID;

/**
 * TransactionRequest includes all the commands need to the executed by the transaction manager
 * within a transaction.
 */
@AutoValue
public abstract class TransactionRequest {
  static Builder builder() {
    return new AutoValue_TransactionRequest.Builder().setPrivacyBudgetLimit(1);
  }

  @AutoValue.Builder
  public abstract static class Builder {
    public abstract Builder setTransactionId(UUID transactionId);

    public abstract Builder setAttributionReportTo(String attributionReportTo);

    public abstract Builder setPrivacyBudgetUnits(
        ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits);

    public abstract Builder setTimeout(Timestamp timeout);

    public abstract Builder setTransactionSecret(String transactionSecret);

    public abstract Builder setPrivacyBudgetLimit(Integer privacyBudgetLimit);

    public abstract TransactionRequest build();
  }

  // Id of the transaction.
  public abstract UUID transactionId();
  // ad-tech origin where reports will be sent
  public abstract String attributionReportTo();
  // List of Privacy budgeting units
  public abstract ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits();
  // Timestamp of when the transaction expires.
  public abstract Timestamp timeout();
  // The secret of the transaction.
  public abstract String transactionSecret();

  // Number of privacy budget units to consume for each key in the transaction
  public abstract Integer privacyBudgetLimit();
  // List of privacy budget units whose budget has exhausted
  private ImmutableList<PrivacyBudgetUnit> exhaustedPrivacyBudgetUnits;
}
