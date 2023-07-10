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

package com.google.scp.coordinator.keymanagement.keyhosting.tasks;

import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keyhosting.tasks.Annotations.KeyLimit;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;

/** Returns all active public keys */
public final class GetActivePublicKeysTask {

  private final KeyDb keyDb;
  private final int keyLimit;

  @Inject
  public GetActivePublicKeysTask(KeyDb keyDb, @KeyLimit Integer keyLimit) {
    this.keyDb = keyDb;
    this.keyLimit = keyLimit;
  }

  /** Gets all public keys from the respective Key database */
  public ImmutableList<EncryptionKey> getActivePublicKeys() throws ServiceException {
    return keyDb.getActiveKeys(keyLimit);
  }
}
