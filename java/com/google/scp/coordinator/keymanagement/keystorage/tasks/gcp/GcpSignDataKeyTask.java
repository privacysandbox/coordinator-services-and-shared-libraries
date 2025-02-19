/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.keymanagement.keystorage.tasks.gcp;

import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.SignDataKeyTask;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.shared.api.exception.ServiceException;

/**
 * Stub SignDataKeyTask implementation.
 *
 * <p>Unusued because GCP's KeyStorageService does not use DataKeys at the moment.
 */
public final class GcpSignDataKeyTask implements SignDataKeyTask {
  @Override
  public DataKey signDataKey(DataKey dataKey) throws ServiceException {
    throw new IllegalStateException("Not implemented");
  }

  @Override
  public void verifyDataKey(DataKey dataKey) throws ServiceException {
    throw new IllegalStateException("Not implemented");
  }
}
