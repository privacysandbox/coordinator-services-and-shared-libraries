package com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp;

import static com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KeyEncryptionKeyUri;
import static com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KmsKeyAead;

import com.google.crypto.tink.Aead;
import com.google.inject.Inject;
import com.google.protobuf.ByteString;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.KeyStorageClient;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.KeyStorageClient.KeyStorageServiceException;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateSplitKeyTaskBase;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.KeyIdFactory;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp.Annotations.PeerCoordinatorKmsKeyAead;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.security.GeneralSecurityException;
import java.time.Instant;
import java.util.Base64;
import java.util.Optional;

/**
 * The entire split key generation task. Handles generating the key split, encrypting the primary
 * split, encrypting the second split with the PeerCoordinatorKmsKeyAead and sending it to
 * Coordinator B, and finally receiving and storing the signed response.
 *
 * <p>The main difference with AWS is the direct encryption of Coordinator B keySplit rather than
 * the use of a DataKey via the KeyExchangeService.
 */
public final class GcpCreateSplitKeyTask extends CreateSplitKeyTaskBase {

  private final Aead peerCoordinatorKmsKeyAead;

  @Inject
  public GcpCreateSplitKeyTask(
      @KmsKeyAead Aead kmsKeyAead,
      @KeyEncryptionKeyUri String keyEncryptionKeyUri,
      @PeerCoordinatorKmsKeyAead Aead peerCoordinatorKmsKeyAead,
      KeyIdFactory keyIdFactory,
      KeyDb keyDb,
      KeyStorageClient keyStorageClient) {
    super(kmsKeyAead, keyEncryptionKeyUri, Optional.empty(), keyDb, keyStorageClient, keyIdFactory);
    this.peerCoordinatorKmsKeyAead = peerCoordinatorKmsKeyAead;
  }

  /**
   * The actual key generation process. Performs the necessary encryption key generation and
   * splitting, key storage request, and database persistence with signatures.
   *
   * @see CreateSplitKeyTaskBase#createSplitKey(int, int, int, Instant)
   */
  public void createSplitKey(int count, int validityInDays, int ttlInDays, Instant activation)
      throws ServiceException {
    createSplitKeyBase(count, validityInDays, ttlInDays, activation, Optional.empty());
  }

  /**
   * Encrypt CoordinatorB KeySplit using Peer Coordinator KMS Key.
   *
   * @param dataKey Unused by design since GCP encrypts the keySplit directly.
   */
  @Override
  protected String encryptPeerCoordinatorSplit(
      ByteString keySplit, Optional<DataKey> dataKey, String publicKeyMaterial)
      throws ServiceException {
    try {
      return Base64.getEncoder()
          .encodeToString(
              peerCoordinatorKmsKeyAead.encrypt(
                  keySplit.toByteArray(), Base64.getDecoder().decode(publicKeyMaterial)));
    } catch (GeneralSecurityException e) {
      throw new ServiceException(Code.INVALID_ARGUMENT, "Failed to encrypt key split.", e);
    }
  }

  /**
   * Send KeySplit to Peer Coordinator using KeyStorageService.
   *
   * @param dataKey Unused by design since GCP does not use a data key to encrypt keySplit. See
   *     {@link GcpCreateSplitKeyTask#encryptPeerCoordinatorSplit}
   */
  @Override
  protected EncryptionKey sendKeySplitToPeerCoordinator(
      EncryptionKey unsignedCoordinatorBKey, String encryptedKeySplitB, Optional<DataKey> dataKey)
      throws ServiceException {
    try {
      return keyStorageClient.createKey(unsignedCoordinatorBKey, encryptedKeySplitB);
    } catch (KeyStorageServiceException e) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT, "Key Storage Service failed to validate, sign, and store key", e);
    }
  }
}
