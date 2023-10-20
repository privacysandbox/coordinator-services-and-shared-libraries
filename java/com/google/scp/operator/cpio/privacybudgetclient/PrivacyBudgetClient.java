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

package com.google.scp.operator.cpio.privacybudgetclient;

import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;

public interface PrivacyBudgetClient {

  /** Consumes a privacy budget request and returns a response. */
  ConsumePrivacyBudgetResponse consumePrivacyBudget(ConsumePrivacyBudgetRequest privacyBudget)
      throws PrivacyBudgetServiceException, PrivacyBudgetClientException;

  /**
   * Represents an exception thrown by the {@code PrivacyBudgetClient} class, for service errors.
   */
  final class PrivacyBudgetServiceException extends Exception {
    /** Creates a new instance of the exception. */
    public PrivacyBudgetServiceException(String message, Throwable cause) {
      super(message, cause);
    }
  }

  /** Represents an exception thrown by the {@code PrivacyBudgetClient} class, for client errors. */
  final class PrivacyBudgetClientException extends Exception {
    /** Creates a new instance of the exception. */
    public PrivacyBudgetClientException(String message) {
      super(message);
    }
  }
}
