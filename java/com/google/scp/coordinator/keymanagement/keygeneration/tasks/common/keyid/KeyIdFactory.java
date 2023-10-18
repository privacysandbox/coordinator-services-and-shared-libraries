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

/** Interface for getting next key id and key id decoding and encoding */
public interface KeyIdFactory {

  /**
   * Generate next key id given the database.
   *
   * @param keyDb the database used to determine the next key id.
   * @return next key id encoded in String.
   * @throws ServiceException
   */
  String getNextKeyId(KeyDb keyDb) throws ServiceException;
}
