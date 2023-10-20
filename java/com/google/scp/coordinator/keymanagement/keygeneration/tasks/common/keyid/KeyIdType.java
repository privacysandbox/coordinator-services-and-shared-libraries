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

import java.util.Optional;

/** Supported key id types. */
public enum KeyIdType {
  UUID(new UuidKeyIdFactory()),
  SEQUENCE(new SequenceKeyIdFactory());
  private final KeyIdFactory keyIdFactory;

  KeyIdType(KeyIdFactory keyIdFactory) {
    this.keyIdFactory = keyIdFactory;
  }

  public static Optional<KeyIdType> fromString(Optional<String> keyIdType) {
    try {
      return Optional.of(keyIdType.map(String::toUpperCase).map(KeyIdType::valueOf).orElse(UUID));
    } catch (IllegalArgumentException e) {
      return Optional.empty();
    }
  }

  public KeyIdFactory getKeyIdFactory() {
    return keyIdFactory;
  }
}
