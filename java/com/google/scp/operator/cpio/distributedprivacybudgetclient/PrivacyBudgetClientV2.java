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

/** Class responsible for making requests to a coordinator to consume budget. */
public interface PrivacyBudgetClientV2 {
  /**
   * Performs a request against the provided baseUrl as part of the HEALTH-CHECK phase of a
   * transaction.
   */
  HealthCheckResponse performActionHealthCheck(TransactionRequest request);

  /**
   * Performs a request against the provided baseUrl as part of the CONSUME-BUDGET phase of a
   * transaction.
   */
  ConsumeBudgetResponse performActionConsumeBudget(TransactionRequest request);

  /**
   * Returns the Base URL of the coordinator against which this client performs the transaction
   * actions.
   */
  String getPrivacyBudgetServerIdentifier();
}
