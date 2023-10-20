package com.google.scp.coordinator.keymanagement.keystorage.tasks.gcp;

import static com.google.scp.coordinator.keymanagement.shared.model.KeyManagementErrorReason.SERVICE_ERROR;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;

import com.google.common.collect.ImmutableList;
import com.google.crypto.tink.Aead;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.KmsKeyAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.KmsKeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.CreateKeyTask;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.security.GeneralSecurityException;
import java.util.Base64;

/** Creates a key in the database */
public final class GcpCreateKeyTask implements CreateKeyTask {

  private final KeyDb keyDb;
  private final Aead kmsKeyAead;
  private final String kmsKeyEncryptionKeyUri;

  @Inject
  public GcpCreateKeyTask(
      KeyDb keyDb,
      @KmsKeyAead Aead kmsKeyAead,
      @KmsKeyEncryptionKeyUri String kmsKeyEncryptionKeyUri) {
    this.keyDb = keyDb;
    this.kmsKeyAead = kmsKeyAead;
    this.kmsKeyEncryptionKeyUri = kmsKeyEncryptionKeyUri;
  }

  /** Creates the key in the database */
  @Override
  public EncryptionKey createKey(EncryptionKey encryptionKey, String encryptedKeySplit)
      throws ServiceException {
    if (encryptedKeySplit.isEmpty()) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT, SERVICE_ERROR.name(), "Key payload is empty.");
    }

    String validatedKeySplit =
        validatedPrivateKeySplit(encryptedKeySplit, encryptionKey.getPublicKeyMaterial());

    var newKeySplitData =
        ImmutableList.<KeySplitData>builder()
            .addAll(encryptionKey.getKeySplitDataList())
            .add(
                KeySplitData.newBuilder()
                    .setKeySplitKeyEncryptionKeyUri(kmsKeyEncryptionKeyUri)
                    .build());
    var newEncryptionKey =
        encryptionKey.toBuilder()
            .setJsonEncodedKeyset(validatedKeySplit)
            .setKeyEncryptionKeyUri(kmsKeyEncryptionKeyUri)
            // Need to clear before adding, otherwise there will be duplicate KeySplitData elements.
            .clearKeySplitData()
            .addAllKeySplitData(newKeySplitData.build())
            .build();

    keyDb.createKey(newEncryptionKey);
    // TODO(b/206030473): Figure out where exactly signing should happen.
    return newEncryptionKey;
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
  private String validatedPrivateKeySplit(String privateKeySplit, String publicKeyMaterial)
      throws ServiceException {
    try {
      byte[] cipherText = Base64.getDecoder().decode(privateKeySplit);
      byte[] associatedData = Base64.getDecoder().decode(publicKeyMaterial);
      byte[] plainText = kmsKeyAead.decrypt(cipherText, associatedData);
      return Base64.getEncoder().encodeToString(kmsKeyAead.encrypt(plainText, new byte[0]));
    } catch (NullPointerException | GeneralSecurityException ex) {
      throw new ServiceException(
          INVALID_ARGUMENT, SERVICE_ERROR.name(), "Key-split validation failed.", ex);
    }
  }
}
