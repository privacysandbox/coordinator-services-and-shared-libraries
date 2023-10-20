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

package com.google.scp.coordinator.keymanagement.keygeneration.app.common;

/** Picker for what decryption stategy for the SplitKeyGenerationApplication to take. */
public enum SplitKeyDecryptionClientSelector {
  /** Uses the kmstool_enclave_cli (which only works in an enclave) to perform decryptions. */
  ENCLAVE,
  /**
   * Uses the AWS SDK KMS client to perform decryptions (works inside and outside an enclave, cannot
   * do attestation).
   */
  NON_ENCLAVE
}
