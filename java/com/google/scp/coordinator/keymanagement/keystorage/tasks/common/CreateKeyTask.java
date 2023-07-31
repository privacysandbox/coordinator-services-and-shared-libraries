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

package com.google.scp.coordinator.keymanagement.keystorage.tasks.common;

import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;

/** Interface for task classes that create a key in the database */
public interface CreateKeyTask {
  /**
   * Creates the key in the database, returning the created and stored key. For operating on keys
   * encrypted directly using the receiving coordinator's encryption key.
   *
   * @param encryptionKey EncryptionKey containing the metadata to copy to the newly created
   *     encryption key. The contained private key material (if any) is ignored.
   * @param encryptedKeySplit The keysplit payload pre-encrypted with a well-known key URI belonging
   *     to the receiving coordinator. Added to the returned and stored EncryptionKey.
   */
  EncryptionKey createKey(EncryptionKey encryptionKey, String encryptedKeySplit)
      throws ServiceException;

  /**
   * Creates the key in the database, returning the created and stored key. For operating on keys
   * encrypted using a {@link DataKey}.
   *
   * @param encryptionKey EncryptionKey containing the metadata to copy to the newly created
   *     encryption key. The contained private key material (if any) is ignored.
   * @param dataKey The DataKey used to encrypt {@code encryptedKeySplit}.
   * @param encryptedKeySplit The keysplit payload encrypted using {@code dataKey}. The keysplit is
   *     decrypted, re-encrypted, and added to the returned and stored EncryptionKey.
   */
  EncryptionKey createKey(EncryptionKey encryptionKey, DataKey dataKey, String encryptedKeySplit)
      throws ServiceException;
}
