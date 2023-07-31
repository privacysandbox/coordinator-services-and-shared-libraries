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

package com.google.scp.shared.api.util;

import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_HTTP_METHOD;
import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_URL_PATH_OR_VARIABLE;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;

import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.HttpMethod;

/** Defines helper methods to use with HTTP requests */
public final class RequestUtil {

  private RequestUtil() {}

  /**
   * Extracts path variable from path given a path template in the format: '/resource/:resourceId'
   * where variableName = resourceId. Throw Service Exception if path does not match template or
   * variable not found
   */
  public static String getVariableFromPath(String template, String path, String variableName)
      throws ServiceException {

    String[] templateParts = template.split("/", -1);
    String[] pathParts = path.split("/", -1);
    if (pathParts.length != templateParts.length) {
      throw newInvalidPathException(String.format("Invalid URL Path: '%s'", path));
    }
    for (int i = 0; i < templateParts.length; i++) {
      String templatePart = templateParts[i];
      if (templatePart.startsWith(":")) {
        if (templatePart.substring(1).equals(variableName)) {
          return pathParts[i];
        } else if (i + 1 == templateParts.length) {
          throw newInvalidPathException(
              String.format("Variable not found in path: '%s'", variableName));
        }
      } else if (!templatePart.equals(pathParts[i])) {
        throw newInvalidPathException(String.format("Invalid URL Path: '%s'", path));
      }
    }
    throw newInvalidPathException(String.format("Variable not found in path: '%s'", variableName));
  }

  /** Validates a string representation of an HTTP Method against an expected {@link HttpMethod} */
  public static void validateHttpMethod(String methodToValidate, HttpMethod expectedMethod)
      throws ServiceException {
    if (!expectedMethod.name().equals(methodToValidate)) {
      throw new ServiceException(
          /* code = */ INVALID_ARGUMENT,
          /* reason = */ INVALID_HTTP_METHOD.name(),
          /* message = */ String.format("Unsupported method: '%s'", methodToValidate));
    }
  }

  private static ServiceException newInvalidPathException(String message) {
    return new ServiceException(
        /* code = */ INVALID_ARGUMENT,
        /* reason = */ INVALID_URL_PATH_OR_VARIABLE.name(),
        /* message = */ message);
  }
}
