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

import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;

/**
 * Interface for a client to communicate with the Key Storage Service to send and store {@link
 * EncryptionKey} key splits and receive signatures in response.
 *
 * <p>TODO(b/242759135): Align on whether service or backend models should be returned by this
 * client.
 */
public interface KeyStorageClient {

  /**
   * Sends a key to key storage service and returns the signed key back as a response.
   *
   * @param encryptedKeySplit The key split to send to this coordinator, encrypted with a
   *     pre-negotiated key encryption key owned by coordinator B.
   */
  EncryptionKey createKey(EncryptionKey encryptionKey, String encryptedKeySplit)
      throws KeyStorageServiceException;

  /**
   * Sends a key to key storage service and returns the signed key back as a response.
   *
   * @param dataKey The DataKey used to encrypt {@code encryptedKeySplit}
   * @param encryptedKeySplit The key split to send to this coordinator, encrypted using {@code
   *     dataKey}
   */
  EncryptionKey createKey(EncryptionKey encryptionKey, DataKey dataKey, String encryptedKeySplit)
      throws KeyStorageServiceException;

  /**
   * Fetches a new Data Key from KeyStorageService to be used to encrypt the "encryptedKeySplit"
   * sent to key storage service using {@link #createKey(EncryptionKey, String)}
   */
  public DataKey fetchDataKey() throws KeyStorageServiceException;

  /**
   * Represents an exception thrown by the {@code KeyStorageClient} class, for service errors from
   * the key storage client.
   */
  final class KeyStorageServiceException extends Exception {
    /** Creates a new instance of the exception. */
    public KeyStorageServiceException(String message, Throwable cause) {
      super(message, cause);
    }

    public KeyStorageServiceException(String message) {
      super(message);
    }
  }
}
