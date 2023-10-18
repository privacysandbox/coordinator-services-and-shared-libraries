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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.aws;

import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.DataKeyEncryptionUtil.encryptWithDataKey;

import com.google.crypto.tink.Aead;
import com.google.crypto.tink.PublicKeySign;
import com.google.inject.Inject;
import com.google.protobuf.ByteString;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.KeyStorageClient;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.KeyStorageClient.KeyStorageServiceException;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.EncryptionKeySignatureKey;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KmsKeyAead;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateSplitKeyTaskBase;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.KeyIdFactory;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.util.Base64Util;
import java.security.GeneralSecurityException;
import java.time.Instant;
import java.util.Optional;

/**
 * The entire split key generation task. Handles fetching an encrypted Key Exchange Key, decrypting
 * it via KMS, generating the key split, sending the second split to Coordinator B, and receiving
 * and storing the signed response.
 */
public final class AwsCreateSplitKeyTask extends CreateSplitKeyTaskBase {

  private final CloudAeadSelector aeadSelector;

  @Inject
  public AwsCreateSplitKeyTask(
      @KmsKeyAead Aead keyEncryptionKeyAead,
      @KeyEncryptionKeyUri String keyEncryptionKeyUri,
      @EncryptionKeySignatureKey Optional<PublicKeySign> signatureKey,
      KeyDb keyDb,
      KeyStorageClient keyStorageClient,
      KeyIdFactory keyIdFactory,
      CloudAeadSelector aeadSelector) {
    super(
        keyEncryptionKeyAead,
        keyEncryptionKeyUri,
        signatureKey,
        keyDb,
        keyStorageClient,
        keyIdFactory);
    this.aeadSelector = aeadSelector;
  }

  /**
   * The actual key generation process. Performs the necessary key exchange key fetching, data
   * encryption key generation and splitting, key storage request, and database persistence with
   * signatures.
   *
   * <p>For key regeneration {@link CreateSplitKeyTaskBase#replaceExpiringKeys(int, int, int)}
   * should be used.
   *
   * @see CreateSplitKeyTaskBase#createSplitKey(int, int, int, Instant)
   */
  @Override
  public void createSplitKey(int count, int validityInDays, int ttlInDays, Instant activation)
      throws ServiceException {
    // Reuse same data key for key batch.
    var dataKey = fetchDataKey();
    createSplitKeyBase(count, validityInDays, ttlInDays, activation, Optional.of(dataKey));
  }

  @Override
  protected String encryptPeerCoordinatorSplit(
      ByteString keySplit, Optional<DataKey> dataKey, String publicKey) throws ServiceException {
    try {
      return Base64Util.toBase64String(
          encryptWithDataKey(aeadSelector, dataKey.get(), keySplit, publicKey));
    } catch (GeneralSecurityException e) {
      throw new ServiceException(Code.INVALID_ARGUMENT, "Failed to encrypt key split.", e);
    }
  }

  @Override
  protected EncryptionKey sendKeySplitToPeerCoordinator(
      EncryptionKey unsignedCoordinatorBKey, String encryptedKeySplitB, Optional<DataKey> dataKey)
      throws ServiceException {
    try {
      return keyStorageClient.createKey(unsignedCoordinatorBKey, dataKey.get(), encryptedKeySplitB);
    } catch (KeyStorageServiceException e) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT, "Key Storage Service failed to validate, sign, and store key", e);
    }
  }

  private DataKey fetchDataKey() throws ServiceException {
    try {
      return keyStorageClient.fetchDataKey();
    } catch (KeyStorageServiceException e) {
      throw new ServiceException(
          Code.INTERNAL, "Failed to fetch data key from key storage service", e);
    }
  }
}
