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

package com.google.scp.coordinator.keymanagement.keystorage.service.common;

import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;

import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.converter.DataKeyConverter;
import com.google.scp.coordinator.keymanagement.keystorage.converters.EncryptionKeyConverter;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.CreateKeyTask;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.GetDataKeyTask;
import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.CreateKeyRequestProto.CreateKeyRequest;
import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.DataKeyProto.DataKey;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;

/** A service with a collection of tasks for saving keys. */
public final class KeyStorageService {

  private final CreateKeyTask createKeyTask;
  private final GetDataKeyTask getDataKeyTask;

  @Inject
  public KeyStorageService(CreateKeyTask createKeyTask, GetDataKeyTask getDataKeyTask) {
    this.createKeyTask = createKeyTask;
    this.getDataKeyTask = getDataKeyTask;
  }

  /**
   * Handles a request to save a key, returning an {@link EncryptionKey} after populating the
   * signature field.
   *
   * <p>TODO(b/206030473): Implement signing. Currently returns the key without signatures.
   */
  public EncryptionKey createKey(CreateKeyRequest request) throws ServiceException {
    try {
      var receivedKey =
          EncryptionKeyConverter.toStorageEncryptionKey(request.getKeyId(), request.getKey());

      com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto
              .EncryptionKey
          storedKey;
      switch (request.getKeySplitEncryptionType()) {
        case UNKNOWN: // fall through.
        case DIRECT:
          storedKey = createKeyTask.createKey(receivedKey, request.getEncryptedKeySplit());
          break;
        case DATA_KEY:
          var dataKey =
              DataKeyConverter.INSTANCE.reverse().convert(request.getEncryptedKeySplitDataKey());
          storedKey = createKeyTask.createKey(receivedKey, dataKey, request.getEncryptedKeySplit());
          break;
        default:
          throw new IllegalArgumentException(
              String.format(
                  "Invalid keySplitEncryptionType (%s)", request.getKeySplitEncryptionType()));
      }

      // TODO(b/206030473): this will eventually also populate with signatures
      return EncryptionKeyConverter.toApiEncryptionKey(storedKey);
    } catch (IllegalArgumentException ex) {
      throw new ServiceException(INVALID_ARGUMENT, INVALID_ARGUMENT.name(), ex.getMessage(), ex);
    }
  }

  /** Gets a new set of data keys used for the split key exchange. */
  public DataKey getDataKey() throws ServiceException {
    return DataKeyConverter.INSTANCE.convert(getDataKeyTask.getDataKey());
  }
}
