/*
 * Copyright 2025 Google LLC
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

package com.google.scp.coordinator.keymanagement.keystorage.tasks.gcp;

import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.SERVICE_ERROR;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;

import com.google.common.collect.ImmutableList;
import com.google.crypto.tink.Aead;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.AwsKeySyncKeyAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.AwsKeySyncKeyDb;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.AwsKeySyncKeyUri;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DecryptionAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.CreateKeyTask;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.security.GeneralSecurityException;
import java.util.Base64;

/**
 * Create and persist a key split into GCP key DB, with syncing to the AWS key DB.
 *
 * <p>NOTE: The following class is of limited time use during the AWS cross-cloud coordinator
 * migration.
 */
public final class GcpCreateKeyWithSyncTask implements CreateKeyTask {

  private final KeyDb keyDb;
  private final Aead decryptionAead;
  private final Aead encryptionAead;
  private final String encryptionKeyUri;

  private final KeyDb keySyncDb;
  private final String keySyncEncryptionKeyUri;
  private final Aead keySyncEncryptionAead;

  private class ReencryptedKeySplits {
    private final String gcpKeySplit;
    private final String awsKeySplit;

    ReencryptedKeySplits(String xccKeySplit, String awsKeySplit) {
      this.gcpKeySplit = xccKeySplit;
      this.awsKeySplit = awsKeySplit;
    }
  }

  @Inject
  public GcpCreateKeyWithSyncTask(
      KeyDb keyDb,
      @DecryptionAead Aead decryptionAead,
      @EncryptionAead Aead encryptionAead,
      @EncryptionKeyUri String encryptionKeyUri,
      @AwsKeySyncKeyDb KeyDb keySyncDb,
      @AwsKeySyncKeyUri String keySyncUri,
      @AwsKeySyncKeyAead Aead keySyncEncryptionAead) {
    this.keyDb = keyDb;
    this.decryptionAead = decryptionAead;
    this.encryptionAead = encryptionAead;
    this.encryptionKeyUri = encryptionKeyUri;
    this.keySyncDb = keySyncDb;
    this.keySyncEncryptionKeyUri = keySyncUri;
    this.keySyncEncryptionAead = keySyncEncryptionAead;
  }

  /** Creates the key and persists into both GCP and AWS key database. */
  @Override
  public EncryptionKey createKey(EncryptionKey encryptionKey, String encryptedKeySplit)
      throws ServiceException {
    if (encryptedKeySplit.isEmpty()) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT, SERVICE_ERROR.name(), "Key payload is empty.");
    }

    ReencryptedKeySplits validatedKeySplit =
        validatedPrivateKeySplit(encryptedKeySplit, encryptionKey.getPublicKeyMaterial());

    String typeAKeySyncKmsUri = encryptionKey.getKeySplitData(1).getKeySplitKeyEncryptionKeyUri();
    finalizeAndPersistEncryptionKey(
        updateKeySplitDataWithKeyURI(encryptionKey, typeAKeySyncKmsUri),
        keySyncEncryptionKeyUri,
        validatedKeySplit.awsKeySplit,
        keySyncDb);

    String typeAKmsUri = encryptionKey.getKeySplitData(0).getKeySplitKeyEncryptionKeyUri();
    EncryptionKey newEncryptionKey =
        finalizeAndPersistEncryptionKey(
            updateKeySplitDataWithKeyURI(encryptionKey, typeAKmsUri),
            encryptionKeyUri,
            validatedKeySplit.gcpKeySplit,
            keyDb);

    // Update key split data to include both AWS and cross-cloud KMS encryption key URIs.
    EncryptionKey encryptionKeyToReturn =
        newEncryptionKey.toBuilder()
            .clearKeySplitData()
            .addKeySplitData(
                KeySplitData.newBuilder().setKeySplitKeyEncryptionKeyUri(encryptionKeyUri).build())
            .addKeySplitData(
                KeySplitData.newBuilder()
                    .setKeySplitKeyEncryptionKeyUri(keySyncEncryptionKeyUri)
                    .build())
            .build();

    return encryptionKeyToReturn;
  }

  @Override
  public EncryptionKey createKey(
      EncryptionKey encryptionKey, DataKey dataKey, String decryptedKeySplit)
      throws ServiceException {
    throw new ServiceException(
        Code.NOT_FOUND, SERVICE_ERROR.name(), "DataKey decryption not implemented");
  }

  /**
   * Attempts to validate a private key split along with the associated public key. Valid private
   * key split is re-encrypted.
   *
   * @return Validated and re-encrypted private key split without associated data.
   * @throw ServiceException if the private key split is invalid.
   */
  private ReencryptedKeySplits validatedPrivateKeySplit(
      String privateKeySplit, String publicKeyMaterial) throws ServiceException {
    try {
      byte[] cipherText = Base64.getDecoder().decode(privateKeySplit);
      byte[] associatedData = Base64.getDecoder().decode(publicKeyMaterial);
      byte[] plainText = decryptionAead.decrypt(cipherText, associatedData);
      return new ReencryptedKeySplits(
          Base64.getEncoder().encodeToString(encryptionAead.encrypt(plainText, new byte[0])),
          Base64.getEncoder()
              .encodeToString(keySyncEncryptionAead.encrypt(plainText, new byte[0])));
    } catch (NullPointerException | GeneralSecurityException ex) {
      throw new ServiceException(
          INVALID_ARGUMENT, SERVICE_ERROR.name(), "Key-split validation failed.", ex);
    }
  }

  private static EncryptionKey finalizeAndPersistEncryptionKey(
      EncryptionKey encryptionKey, String encryptionKeyUri, String validatedSplit, KeyDb keyDb)
      throws ServiceException {
    var newKeySplitDataBuilder =
        ImmutableList.<KeySplitData>builder()
            .addAll(encryptionKey.getKeySplitDataList())
            .add(
                KeySplitData.newBuilder().setKeySplitKeyEncryptionKeyUri(encryptionKeyUri).build());
    var newEncryptionKey =
        encryptionKey.toBuilder()
            .setJsonEncodedKeyset(validatedSplit)
            .setKeyEncryptionKeyUri(encryptionKeyUri)
            .clearKeySplitData()
            .addAllKeySplitData(newKeySplitDataBuilder.build())
            .build();
    keyDb.createKey(newEncryptionKey);
    return newEncryptionKey;
  }

  private static EncryptionKey updateKeySplitDataWithKeyURI(EncryptionKey key, String kmsKey) {
    return key.toBuilder()
        .clearKeySplitData()
        .addKeySplitData(KeySplitData.newBuilder().setKeySplitKeyEncryptionKeyUri(kmsKey).build())
        .build();
  }
}
