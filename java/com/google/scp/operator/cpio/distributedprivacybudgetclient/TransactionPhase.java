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

/** Supported states for a Transaction phase. */
public enum TransactionPhase {
  @Deprecated
  NOTSTARTED,
  @Deprecated
  BEGIN,
  @Deprecated
  PREPARE,
  @Deprecated
  COMMIT,
  @Deprecated
  NOTIFY,
  @Deprecated
  ABORT,
  @Deprecated
  END,
  @Deprecated
  UNKNOWN,
  @Deprecated
  FINISHED,
  HEALTH_CHECK,
  CONSUME_BUDGET;
}
