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

import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;

public interface DistributedPrivacyBudgetClient {

  /**
   * @param request Privacy budget request
   * @return Response for consuming the privacy budget
   * @throws DistributedPrivacyBudgetClientException
   * @throws DistributedPrivacyBudgetServiceException
   */
  ConsumePrivacyBudgetResponse consumePrivacyBudget(ConsumePrivacyBudgetRequest request)
      throws DistributedPrivacyBudgetClientException, DistributedPrivacyBudgetServiceException;

  /**
   * Represents an exception thrown by the {@code DistributedPrivacyBudgetClient} class, for service
   * errors.
   */
  final class DistributedPrivacyBudgetServiceException extends Exception {
    private final StatusCode statusCode;

    /** Creates a new instance of the exception. */
    public DistributedPrivacyBudgetServiceException(StatusCode statusCode, Throwable cause) {
      super(statusCode.name(), cause);
      this.statusCode = statusCode;
    }

    public StatusCode getStatusCode() {
      return this.statusCode;
    }
  }

  /**
   * Represents an exception thrown by the {@code DistributedPrivacyBudgetClient} class, for client
   * errors.
   */
  final class DistributedPrivacyBudgetClientException extends Exception {
    /** Creates a new instance of the exception. */
    public DistributedPrivacyBudgetClientException(String message) {
      super(message);
    }
  }
}
