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

import com.google.crypto.tink.Aead;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.CoordinatorKekUri;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.CoordinatorKeyAead;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.util.Base64Util;
import com.google.scp.shared.util.EncryptionUtil;
import java.io.IOException;
import java.security.GeneralSecurityException;

/** Task for retrieving a new exchange key */
public class GetDataKeyTask {

  private final String coordinatorKekUri;
  private final EncryptionUtil encryptionService;
  private final Aead aead;
  private final SignDataKeyTask signDataKeyTask;

  @Inject
  public GetDataKeyTask(
      EncryptionUtil encryptionService,
      SignDataKeyTask signDataKeyTask,
      @CoordinatorKeyAead Aead aead,
      @CoordinatorKekUri String coordinatorKekUri) {
    this.encryptionService = encryptionService;
    this.aead = aead;
    this.coordinatorKekUri = coordinatorKekUri;
    this.signDataKeyTask = signDataKeyTask;
  }

  /** Gets a new DataKey */
  public DataKey getDataKey() throws ServiceException {
    try {
      String encryptedDataKey =
          Base64Util.toBase64String(encryptionService.generateSymmetricEncryptedKey(aead));

      var unsignedDataKey =
          DataKey.newBuilder()
              .setEncryptedDataKey(encryptedDataKey)
              .setEncryptedDataKeyKekUri(coordinatorKekUri)
              .build();

      return signDataKeyTask.signDataKey(unsignedDataKey);
    } catch (GeneralSecurityException | IOException e) {
      throw new ServiceException(Code.INTERNAL, e.getMessage(), e);
    }
  }
}
