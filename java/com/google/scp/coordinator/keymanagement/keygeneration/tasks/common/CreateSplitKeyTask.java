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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.common;

import com.google.scp.shared.api.exception.ServiceException;

/** Interface for task classes that generate split keys */
public interface CreateSplitKeyTask {

  /**
   * The actual key generation process. Performs the necessary key exchange key fetching (if
   * applicable), encryption key generation and splitting, key storage request, and database
   * persistence with signatures.
   *
   * <p>For key regeneration {@link CreateSplitKeyTask#replaceExpiringKeys(int, int, int)} should be
   * used.
   */
  void createSplitKey(int count, int validityInDays, int ttlInDays) throws ServiceException;

  /**
   * Counts the number of active keys in the KeyDB and creates enough keys to both replace any keys
   * expiring soon and replace any missing keys.
   *
   * <p>The generated keys will expire after validityInDays days, will have a Time-to-Live of
   * ttlInDays, and will be stored in the KeyDB.
   */
  void replaceExpiringKeys(int numDesiredKeys, int validityInDays, int ttlInDays)
      throws ServiceException;
}
