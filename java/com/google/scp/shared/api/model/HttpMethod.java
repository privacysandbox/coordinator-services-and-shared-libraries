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

package com.google.scp.shared.api.model;

/**
 * Enum representation of methods listed under HttpRule in
 * https://github.com/googleapis/api-common-protos/blob/master/google/api/http.proto
 */
public enum HttpMethod {
  /** Used for listing and getting information about resources. */
  GET,

  /** Used for updating a resource. */
  PUT,

  /** Used for creating a resource. */
  POST,

  /** Used for deleting a resource. */
  DELETE,

  /** Used for updating a resource. */
  PATCH;

  /** Return whether the http method is valid. */
  public static boolean isValid(String method) {
    HttpMethod[] httpMethods = HttpMethod.values();
    for (HttpMethod httpMethod : httpMethods) if (httpMethod.name().equals(method)) return true;
    return false;
  }
}
