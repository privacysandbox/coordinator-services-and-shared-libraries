/*
 * Copyright 2023 Google LLC
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

package com.google.scp.coordinator.keymanagement.keyhosting.tasks;

import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.time.Duration;
import java.util.stream.Stream;

/**
 * This task retrieves all recently created {@link EncryptionKey}s up to a specific age from {@link
 * KeyDb}.
 */
public final class ListRecentEncryptionKeysTask {
  private final KeyDb keyDb;

  @Inject
  public ListRecentEncryptionKeysTask(KeyDb keyDb) {
    this.keyDb = keyDb;
  }

  /**
   * all recently created {@link EncryptionKey}s up to a specific age.
   *
   * @param maxAgeSeconds Maximum age of {@link EncryptionKey} that should be returned.
   */
  public Stream<EncryptionKey> execute(int maxAgeSeconds) throws ServiceException {
    if (maxAgeSeconds < 0) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          KeyManagementErrorReason.INVALID_ARGUMENT.name(),
          String.format("Age should be positive, found (%s) instead.", maxAgeSeconds));
    }
    return keyDb.listRecentKeys(Duration.ofSeconds(maxAgeSeconds));
  }
}
