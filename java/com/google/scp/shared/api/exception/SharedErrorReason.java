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

/** Defined error reasons to be used with shared methods */
public enum SharedErrorReason {
  /** Error when url path is invalid or path variable missing */
  INVALID_URL_PATH_OR_VARIABLE,
  /** Error when request is unsupported HTTP method */
  INVALID_HTTP_METHOD,
  /** An internal system error occurred */
  SERVER_ERROR,
  /** An input argument was invalid */
  INVALID_ARGUMENT,
  /** Not authorized to perform request */
  NOT_AUTHORIZED
}
