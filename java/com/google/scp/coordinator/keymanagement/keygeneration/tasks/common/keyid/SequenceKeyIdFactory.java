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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid;

import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.shared.api.exception.ServiceException;
import java.time.Duration;

/** Sequence key id factory used to generate, encode, and decode sequence key id. */
public final class SequenceKeyIdFactory implements KeyIdFactory {

  public Long decodeKeyIdFromString(String keyId) {
    return Long.reverse(Long.parseUnsignedLong(keyId, 16));
  }

  public String encodeKeyIdToString(Long keyId) {
    StringBuilder stringBuilder = new StringBuilder();
    String key = Long.toHexString(Long.reverse(keyId));
    stringBuilder.append("0".repeat(Math.max(0, 16 - key.length())));
    stringBuilder.append(key);
    return stringBuilder.toString().toUpperCase();
  }

  @Override
  public String getNextKeyId(KeyDb keyDb) throws ServiceException {
    return encodeKeyIdToString(
        keyDb
            // List all keys that were created in the past 7 weeks.
            // This is longer than the duration that keys will be used.
            // Allows for recovering non-production environments that don't
            // have monitoring of key generation issues and may have failed
            // for a while.
            .listRecentKeys(Duration.ofHours(1176))
            .map(a -> decodeKeyIdFromString(a.getKeyId()))
            .sorted()
            // Find the value of the first keyId where incrementing it by 1
            // does not equal the next.
            .reduce((i, j) -> i + 1 == j ? j : i)
            // There should be a single keyId remaining - increment it by 1
            .map(i -> i + 1)
            // If there are no keys, return 0
            .orElse(0L));
  }
}
