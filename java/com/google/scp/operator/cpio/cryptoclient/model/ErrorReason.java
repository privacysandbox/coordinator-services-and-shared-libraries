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

package com.google.scp.operator.cpio.cryptoclient.model;

/** Error reasons for crypto client. */
public enum ErrorReason {
  // Internal error, caused by unexpected server-side exceptions.
  INTERNAL,
  // Unable to decrypt key, caused by error in KMS tool
  KEY_DECRYPTION_ERROR,
  // Key not found error, caused by HTTP 404 error on private key endpoint when key is not found
  KEY_NOT_FOUND,
  // Permission was denied, caused by HTTP 403 error on private key endpoint.
  PERMISSION_DENIED,
  // An unknown fatal error occurred.
  UNKNOWN_ERROR,
  // Service unresponsive or not available.
  KEY_SERVICE_UNAVAILABLE,
}
