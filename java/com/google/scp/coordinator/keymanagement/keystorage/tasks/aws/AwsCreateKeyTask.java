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

package com.google.scp.coordinator.keymanagement.keystorage.tasks.aws;

import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.DataKeyEncryptionUtil.decryptWithDataKey;
import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.SERVICE_ERROR;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;

import com.google.crypto.tink.Aead;
import com.google.crypto.tink.PublicKeySign;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeySignatureKey;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.KmsKeyAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.WorkerKekUri;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.CreateKeyTask;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.SignDataKeyTask;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.util.KeySplitDataUtil;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.util.Base64Util;
import java.security.GeneralSecurityException;
import java.util.Base64;
import java.util.Optional;

/** Creates a key in the database */
public final class AwsCreateKeyTask implements CreateKeyTask {
  private final KeyDb keyDb;
  private final Aead kmsKeyAead;
  private final String keyEncryptionKeyUri;
  private final CloudAeadSelector aeadSelector;
  private final SignDataKeyTask signDataKeyTask;
  private final Optional<PublicKeySign> signatureKey;

  @Inject
  public AwsCreateKeyTask(
      KeyDb keyDb,
      @KmsKeyAead Aead kmsKeyAead,
      @WorkerKekUri String keyEncryptionKeyUri,
      @EncryptionKeySignatureKey Optional<PublicKeySign> signatureKey,
      CloudAeadSelector aeadSelector,
      SignDataKeyTask signDataKeyTask) {
    this.keyDb = keyDb;
    this.kmsKeyAead = kmsKeyAead;
    this.keyEncryptionKeyUri = keyEncryptionKeyUri;
    this.signatureKey = signatureKey;
    this.aeadSelector = aeadSelector;
    this.signDataKeyTask = signDataKeyTask;
  }

  @Override
  @Deprecated
  public EncryptionKey createKey(EncryptionKey encryptionKey, String receivedKeySplit)
      throws ServiceException {
    // DataKeys must be used to ensure that the incoming key was generated inside an enclave.
    throw new ServiceException(
        Code.INVALID_ARGUMENT,
        SERVICE_ERROR.name(),
        "Unsupported operation: incoming keys must be encrypted with a DataKey.");
  }

  @Override
  public EncryptionKey createKey(
      EncryptionKey encryptionKey, DataKey dataKey, String receivedKeySplit)
      throws ServiceException {

    signDataKeyTask.verifyDataKey(dataKey);

    // Decrypt with data key and re-encrypt with kmsKeyAead.
    var encryptedKeySplit =
        encryptKeySplit(
            kmsKeyAead,
            decryptKeySplit(dataKey, receivedKeySplit, encryptionKey.getPublicKeyMaterial()));
    var newEncryptionKey = addSplitToKey(encryptionKey, encryptedKeySplit);

    keyDb.createKey(newEncryptionKey);

    return newEncryptionKey;
  }

  /** Encrypts and re-base64-encodes the given base64-encoded payload using {@code kmsKeyAead}. */
  private static String encryptKeySplit(Aead encryptionAead, String decryptedKeySplit)
      throws ServiceException {
    if (decryptedKeySplit.isEmpty()) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT, SERVICE_ERROR.name(), "Key payload is empty.");
    }

    var decryptedBytes = Base64.getDecoder().decode(decryptedKeySplit);
    try {
      return Base64.getEncoder()
          .encodeToString(encryptionAead.encrypt(decryptedBytes, new byte[0]));
    } catch (GeneralSecurityException ex) {
      throw new ServiceException(INVALID_ARGUMENT, SERVICE_ERROR.name(), ex.getMessage(), ex);
    }
  }

  /**
   * Decrypts the base64-encoded key split using the provided data key and returns the
   * base64-encoded result.
   */
  private String decryptKeySplit(DataKey dataKey, String encryptedKeySplit, String publicKey)
      throws ServiceException {
    try {
      var decryptedBytes =
          decryptWithDataKey(
              aeadSelector, dataKey, Base64Util.fromBase64String(encryptedKeySplit), publicKey);
      return Base64Util.toBase64String(decryptedBytes);
    } catch (GeneralSecurityException e) {
      throw new ServiceException(INVALID_ARGUMENT, SERVICE_ERROR.name(), e.getMessage(), e);
    }
  }
  /**
   * Returns a new EncryptionKey with the provided keysplit added to the top level. Also adds {@link
   * #keyEncryptionKeyUri} to the new encryption key to indicate how this key was encrypted.
   *
   * @param encryptedKeySplit The base64-encoded keysplit encrypted using {@link
   *     #keyEncryptionKeyUri}.
   */
  private EncryptionKey addSplitToKey(EncryptionKey encryptionKey, String encryptedKeySplit)
      throws ServiceException {
    EncryptionKey localKey =
        encryptionKey.toBuilder()
            .setJsonEncodedKeyset(encryptedKeySplit)
            .setKeyEncryptionKeyUri(keyEncryptionKeyUri)
            .build();
    try {
      return KeySplitDataUtil.addKeySplitData(localKey, keyEncryptionKeyUri, signatureKey);
    } catch (GeneralSecurityException e) {
      String msg = "Error generating public key signature";
      throw new ServiceException(Code.INTERNAL, "CRYPTO_ERROR", msg, e);
    }
  }
}
