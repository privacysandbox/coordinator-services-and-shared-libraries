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

/** Operation's execution result with status and status code. */
@AutoValue
public abstract class ExecutionResult {

  /**
   * @param executionStatus Operation's execution status.
   * @param statusCode Status code returned from operation execution.
   * @return Instance of ExecutionResult class.
   */
  static ExecutionResult create(ExecutionStatus executionStatus, StatusCode statusCode) {
    return new AutoValue_ExecutionResult(executionStatus, statusCode);
  }

  abstract ExecutionStatus executionStatus();

  abstract StatusCode statusCode();
}
