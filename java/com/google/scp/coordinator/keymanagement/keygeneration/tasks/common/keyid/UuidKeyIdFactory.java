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
import java.util.UUID;

/** UUID key id factory that supports generate, encode and decode UUID key id. */
public final class UuidKeyIdFactory implements KeyIdFactory {

  @Override
  public String getNextKeyId(KeyDb keyDb) {
    return UUID.randomUUID().toString();
  }
}
