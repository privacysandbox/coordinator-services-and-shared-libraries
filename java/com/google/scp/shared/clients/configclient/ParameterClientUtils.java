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

import static com.google.scp.shared.clients.configclient.model.ErrorReason.INVALID_PARAMETER_NAME;
import static java.util.stream.Collectors.joining;

import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import java.util.Optional;
import java.util.stream.Stream;

/** Shared utility functions used by implementations of {@link ParameterClient}. */
public final class ParameterClientUtils {

  /**
   * Builds the full parameter name used by the underlying parameter storage layer.
   *
   * <p>The name is formatted as `{prefix}-{environment}-{parameter}`, with relevant tokens omitted
   * if not provided within the Optional.
   *
   * @param parameter the name of the parameter to expand
   * @param prefix an optional prefix to add to the start of the parameter
   * @param environment an optional environment to add to the middle of the parameter
   * @throws ParameterClientException if the parameter name is invalid
   * @return the storage parameter name
   */
  public static String getStorageParameterName(
      String parameter, Optional<String> prefix, Optional<String> environment)
      throws ParameterClientException {
    if (parameter.isEmpty()) {
      throw new ParameterClientException(
          "Parameter names must not be empty.", INVALID_PARAMETER_NAME);
    }

    String storageParameter =
        Stream.of(prefix.orElse(""), environment.orElse(""), parameter)
            .filter(e -> !e.isEmpty())
            .collect(joining("-"));

    validateParameterName(storageParameter);
    return storageParameter;
  }

  /**
   * Ensures that a parameter name is correctly formatted, throwing an exception if the name is
   * invalid.
   *
   * <p>Valid parameter names are limited to 255 characters, and must consist of only alphanumeric
   * characters, dashes, or underscores.
   *
   * @param param the parameter name to validate
   * @throws ParameterClientException if the parameter name is invalid
   */
  public static void validateParameterName(String param) throws ParameterClientException {
    if (param.isEmpty()) {
      throw new ParameterClientException(
          "Parameter names must not be empty.", INVALID_PARAMETER_NAME);
    }

    // GCP secrets manager limits secret IDs to 255 characters
    if (param.length() > 255) {
      throw new ParameterClientException(
          String.format("Parameter '%s' must be limited to 255 characters.", param),
          INVALID_PARAMETER_NAME);
    }

    // GCP secrets manager only allows alphanumberic, dashes, and underscores:
    // https://cloud.google.com/secret-manager/docs/reference/rpc/google.cloud.secretmanager.v1#createsecretrequest
    if (!param.matches("[A-Za-z0-9_-]+")) {
      throw new ParameterClientException(
          String.format(
              "Parameter '%s' must only contain alphanumeric characters, dashes, or underscores.",
              param),
          INVALID_PARAMETER_NAME);
    }
  }
}
