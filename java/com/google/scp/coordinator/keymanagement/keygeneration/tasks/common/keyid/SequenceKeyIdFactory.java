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
            .listRecentKeys(Duration.ofHours(336))
            .map(a -> decodeKeyIdFromString(a.getKeyId()))
            .sorted()
            .reduce((i, j) -> i + 1 == j ? j : i)
            .map(i -> i + 1)
            .orElse(0L));
  }
}
