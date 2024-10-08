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

/** Status code returned from the execution of an operation. */
public enum StatusCode {
  UNKNOWN,
  OK,
  MALFORMED_DATA,
  @Deprecated
  MAP_ENTRY_DOES_NOT_EXIST,
  @Deprecated
  TRANSACTION_MANAGER_RETRIES_EXCEEDED,
  @Deprecated
  TRANSACTION_ENGINE_CANNOT_START_TRANSACTION,
  @Deprecated
  TRANSACTION_ENGINE_TRANSACTION_IS_EXPIRED,
  @Deprecated
  TRANSACTION_ENGINE_TRANSACTION_FAILED,
  @Deprecated
  TRANSACTION_MANAGER_NOT_FINISHED,
  @Deprecated
  TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE,
  @Deprecated
  TRANSACTION_MANAGER_TRANSACTION_UNKNOWN,
  PRIVACY_BUDGET_CLIENT_UNAUTHENTICATED,
  PRIVACY_BUDGET_CLIENT_UNAUTHORIZED,
  PRIVACY_BUDGET_CLIENT_BUDGET_EXHAUSTED,
  TRANSACTION_ORCHESTRATOR_HEALTH_CHECK_FAILURE,
  TRANSACTION_ORCHESTRATOR_CONSUME_BUDGET_FAILURE,
}
