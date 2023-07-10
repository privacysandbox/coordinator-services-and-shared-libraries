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

package com.google.scp.shared.clients.configclient;

import com.google.scp.shared.clients.configclient.model.ErrorReason;
import java.util.Optional;

/** Interface for fetching parameters. */
public interface ParameterClient {

  /**
   * Blocking call to get a parameter.
   *
   * <p>The parameter storage layer will be invoked in the format: `scp-{environment}-{param}`.
   *
   * @throws ParameterClientException if an error occurred while attempting to read the parameter
   * @return an {@link Optional} of {@link String} for parameter value
   */
  Optional<String> getParameter(String param) throws ParameterClientException;

  /**
   * Blocking call to get a parameter.
   *
   * <p>The parameter storage layer will be invoked in the format:
   * `{paramPrefix}-{environment}-{param}`, with the inclusion of the parameter prefix and the
   * environment controlled by the arguments.
   *
   * @param param the name of the parameter
   * @param paramPrefix an Optional containing the prefix to use in front of the parameter if
   *     present. If not present, no prefix will be added
   * @param includeEnvironmentParam determines whether or not to include the environment in the
   *     parameter string
   * @throws ParameterClientException if an error occurred while attempting to read the parameter
   * @return an {@link Optional} of {@link String} for parameter value
   */
  Optional<String> getParameter(
      String param, Optional<String> paramPrefix, boolean includeEnvironmentParam)
      throws ParameterClientException;

  /**
   * Blocking call to get the environment name.
   *
   * @return an {@link Optional} of {@link String} for environment name
   */
  Optional<String> getEnvironmentName() throws ParameterClientException;

  /** Represents an exception thrown by the {@code ParameterClient} class. */
  final class ParameterClientException extends Exception {
    private final ErrorReason reason;

    /** Creates a new instance from a reason and a {@code Throwable}. */
    public ParameterClientException(ErrorReason reason, Throwable cause) {
      super(cause);
      this.reason = reason;
    }

    /** Creates a new instance from a message String and a reason. */
    public ParameterClientException(String message, ErrorReason reason) {
      super(message);
      this.reason = reason;
    }

    /** Constructs a new exception with the specified detail message, reason, and cause. */
    public ParameterClientException(String message, ErrorReason reason, Throwable cause) {
      super(message, cause);
      this.reason = reason;
    }

    /** Returns the {@link ErrorReason} for the exception. */
    public ErrorReason getReason() {
      return reason;
    }

    /** Returns a String representation of the error. */
    @Override
    public String toString() {
      return String.format("%s (Error reason: %s)", super.toString(), reason);
    }
  }
}
