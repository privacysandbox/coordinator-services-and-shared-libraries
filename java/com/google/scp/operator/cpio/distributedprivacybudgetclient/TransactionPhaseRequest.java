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
import java.sql.Timestamp;
import java.util.UUID;

/** Provides transaction phase execution capabilities for the remotely coordinated transactions. */
@AutoValue
public abstract class TransactionPhaseRequest {

  static Builder builder() {
    return new AutoValue_TransactionPhaseRequest.Builder()
        .setTransactionPhase(TransactionPhase.UNKNOWN);
  }

  @AutoValue.Builder
  abstract static class Builder {
    abstract Builder setTransactionId(UUID transactionId);

    abstract Builder setTransactionPhase(TransactionPhase transactionPhase);

    abstract Builder setLastExecutionTimestamp(Timestamp lastExecutionTimestamp);

    abstract TransactionPhaseRequest build();
  }

  // Id of the transaction.
  public abstract UUID transactionId();
  // The transaction phase to be executed.
  public abstract TransactionPhase transactionPhase();
  // The last execution time stamp of any phases of a transaction.
  public abstract Timestamp lastExecutionTimestamp();
}
