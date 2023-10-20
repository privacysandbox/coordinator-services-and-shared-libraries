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

package com.google.scp.coordinator.keymanagement.keystorage.tasks.common;

import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.shared.api.exception.ServiceException;

/**
 * Creates and validates signatures for {@link DataKey}s
 *
 * <p>Signatures are generated in the process of creating a new data key returned by getDataKey and
 * those signatures are validated by future requests to createKey.
 */
public interface SignDataKeyTask {

  /**
   * Returns a new DataKey with a context and MAC signature added, allowing the returned DataKey to
   * be used with {@code verifyDataKey}.
   */
  public DataKey signDataKey(DataKey dataKey) throws ServiceException;

  /**
   * Validates that the provided data key has a proper signature and has not expired, throwing if
   * validation of either of those fields fails.
   */
  public void verifyDataKey(DataKey dataKey) throws ServiceException;
}
