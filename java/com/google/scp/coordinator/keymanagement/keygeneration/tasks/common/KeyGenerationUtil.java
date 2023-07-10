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

import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.shared.api.exception.ServiceException;
import java.time.Duration;
import java.time.Instant;

/** Shared utility functions between single-party and multi-party key generators. */
public final class KeyGenerationUtil {

  private KeyGenerationUtil() {}

  /**
   * Returns the number of keys that already exist and are not due to be refreshed.
   *
   * @param maxKeys the maximum number of keys to count -- used to limit the number of keys queried
   *     from the database for performance reasons.
   */
  public static int countExistingValidKeys(KeyDb keyDb, Duration keyRefreshWindow, int maxKeys)
      throws ServiceException {
    var refreshIfBefore = Instant.now().plus(keyRefreshWindow);
    return (int)
        keyDb.getActiveKeys(maxKeys).stream()
            .filter(key -> key.getExpirationTime() > refreshIfBefore.toEpochMilli())
            .count();
  }
}
