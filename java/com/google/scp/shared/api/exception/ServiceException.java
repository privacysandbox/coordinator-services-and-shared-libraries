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

package com.google.scp.shared.api.exception;

import com.google.scp.shared.api.model.Code;

/**
 * Wraps service requests exceptions with {@link Code} and a constant-value string to define error
 * reason.
 */
public final class ServiceException extends Exception {

  private final Code code;
  private final String errorReason;

  public ServiceException(Code code, String errorReason, String message) {
    super(message);
    this.code = code;
    this.errorReason = errorReason;
  }

  public ServiceException(Code code, String errorReason, Throwable cause) {
    super(cause);
    this.code = code;
    this.errorReason = errorReason;
  }

  public ServiceException(Code code, String errorReason, String message, Throwable cause) {
    super(message, cause);
    this.code = code;
    this.errorReason = errorReason;
  }

  /** Creates a Service exception for an exception with unknown cause */
  public static ServiceException ofUnknownException(Throwable cause) {
    return new ServiceException(
        Code.UNKNOWN,
        SharedErrorReason.SERVER_ERROR.toString(),
        "An unknown error occurred",
        cause);
  }

  public Code getErrorCode() {
    return code;
  }

  public String getErrorReason() {
    return errorReason;
  }
}
