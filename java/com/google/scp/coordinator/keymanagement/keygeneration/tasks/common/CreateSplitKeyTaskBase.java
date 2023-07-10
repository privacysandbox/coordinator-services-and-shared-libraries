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

package com.google.scp.coordinator.keymanagement.keygeneration.tasks.common;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.common.collect.ImmutableMap.toImmutableMap;
import static com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.KeyGenerationUtil.countExistingValidKeys;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toJsonCleartext;

import com.google.common.collect.ImmutableList;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.PublicKeySign;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.protobuf.ByteString;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.KeyStorageClient;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.keymanagement.shared.util.KeySplitDataUtil;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyTypeProto.EncryptionKeyType;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyStatusProto.EncryptionKeyStatus;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.util.KeyParams;
import com.google.scp.shared.util.KeySplitUtil;
import com.google.scp.shared.util.PublicKeyConversionUtil;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Base64;
import java.util.List;
import java.util.Optional;
import java.util.UUID;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Shared base class between cloud providers for Split Key Generation. Handles generating the key
 * split, sending the second split to Coordinator B, and receiving and storing the signed response.
 */
public abstract class CreateSplitKeyTaskBase implements CreateSplitKeyTask {

  /**
   * Amount of time a key must be valid for to not be refreshed. Keys that expire before (now +
   * keyRefreshWindow) should be replaced with a new key.
   */
  private static final Duration KEY_REFRESH_WINDOW = Duration.ofDays(1);

  private static final Logger LOGGER = LoggerFactory.getLogger(CreateSplitKeyTaskBase.class);

  protected final Aead keyEncryptionKeyAead;
  protected final String keyEncryptionKeyUri;
  protected final Optional<PublicKeySign> signatureKey;
  protected final KeyDb keyDb;
  protected final KeyStorageClient keyStorageClient;

  static {
    try {
      HybridConfig.register();
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Error initializing tink.");
    }
  }

  public CreateSplitKeyTaskBase(
      Aead keyEncryptionKeyAead,
      String keyEncryptionKeyUri,
      Optional<PublicKeySign> signatureKey,
      KeyDb keyDb,
      KeyStorageClient keyStorageClient) {
    this.keyEncryptionKeyAead = keyEncryptionKeyAead;
    this.keyEncryptionKeyUri = keyEncryptionKeyUri;
    this.signatureKey = signatureKey;
    this.keyDb = keyDb;
    this.keyStorageClient = keyStorageClient;
  }

  /**
   * Encrypts a key split for Coordinator B and returns it as an Encrypted key. Optionally use
   * dataKey to encrypt.
   *
   * @throws ServiceException in case of encryption errors.
   */
  protected abstract String encryptPeerCoordinatorSplit(
      ByteString keySplit, Optional<DataKey> dataKey, String publicKey) throws ServiceException;

  /**
   * Takes in an encrypted KeySplit for Coordinator B and an unsignedKey to send to the KeyStorage
   * service for database storage and verification respectively. Returns an EncryptionKey with the
   * keystorage response. Optionally creates a key with a dataKey.
   *
   * @throws ServiceException in case of key storage errors.
   */
  protected abstract EncryptionKey sendKeySplitToPeerCoordinator(
      EncryptionKey unsignedCoordinatorBKey, String encryptedKeySplitB, Optional<DataKey> dataKey)
      throws ServiceException;

  /**
   * Counts the number of active keys in the KeyDB and creates enough keys to both replace any keys
   * expiring soon and replace any missing keys.
   *
   * <p>The generated keys will expire after validityInDays days, will have a Time-to-Live of
   * ttlInDays, and will be stored in the KeyDB.
   */
  public void replaceExpiringKeys(int numDesiredKeys, int validityInDays, int ttlInDays)
      throws ServiceException {
    var numExistingKeys = countExistingValidKeys(keyDb, KEY_REFRESH_WINDOW, numDesiredKeys);
    var numKeysToCreate = numDesiredKeys - numExistingKeys;

    if (numKeysToCreate <= 0) {
      LOGGER.info(String.format("Found %d valid keys, skipping key creation", numExistingKeys));
      return;
    }

    createSplitKey(numKeysToCreate, validityInDays, ttlInDays);
  }

  /**
   * Key generation process. Performs encryption key generation and splitting, key storage request,
   * and database persistence with signatures. Coordinator B encryption and key storage creation are
   * handled by abstract methods implemented in each cloud provider.
   *
   * @param dataKey Passed to encryptPeerCoordinatorSplit and sendKeySplitToPeerCoordinator as
   *     needed for each cloud provider.
   */
  protected void createSplitKeyBase(
      int count, int validityInDays, int ttlInDays, Optional<DataKey> dataKey)
      throws ServiceException {
    Instant creationTime = Instant.now();

    for (int i = 0; i < count; i++) {
      EncryptionKey unsignedCoordinatorAKey;
      EncryptionKey unsignedCoordinatorBKey;
      String encryptedKeySplitB;
      try {
        var keyTemplate = KeyParams.getDefaultKeyTemplate();
        KeysetHandle privateKeysetHandle = KeysetHandle.generateNew(keyTemplate);
        KeysetHandle publicKeysetHandle = privateKeysetHandle.getPublicKeysetHandle();

        ImmutableList<ByteString> keySplits = KeySplitUtil.xorSplit(privateKeysetHandle, 2);

        EncryptionKey key =
            buildEncryptionKey(
                creationTime,
                validityInDays,
                ttlInDays,
                publicKeysetHandle,
                keyEncryptionKeyUri,
                signatureKey);
        unsignedCoordinatorAKey =
            createCoordinatorAKey(keySplits.get(0), key, keyEncryptionKeyAead, keyEncryptionKeyUri);
        unsignedCoordinatorBKey = createCoordinatorBKey(key);

        encryptedKeySplitB =
            encryptPeerCoordinatorSplit(keySplits.get(1), dataKey, key.getPublicKeyMaterial());
      } catch (GeneralSecurityException | IOException e) {
        String msg = "Error generating keys.";
        throw new ServiceException(Code.INTERNAL, "CRYPTO_ERROR", msg, e);
      }

      // Send Coordinator B its key split
      EncryptionKey partyBResponse =
          sendKeySplitToPeerCoordinator(unsignedCoordinatorBKey, encryptedKeySplitB, dataKey);

      // Accumulate signatures and store to KeyDB
      EncryptionKey signedCoordinatorAKey =
          unsignedCoordinatorAKey.toBuilder()
              // Need to clear KeySplitData before adding the combined KeySplitData list.
              .clearKeySplitData()
              .addAllKeySplitData(
                  combineKeySplitData(
                      partyBResponse.getKeySplitDataList(),
                      unsignedCoordinatorAKey.getKeySplitDataList()))
              .build();

      // Note: We want to store the keys as they are generated and signed, so the call to keyDb is
      // inside the loop.  If we run into an exception halfway through count, we want the
      // already-generated keys to be stored.
      keyDb.createKey(signedCoordinatorAKey);
    }
    LOGGER.info(String.format("Successfully generated %s keys", count));
  }

  /**
   * Create unsigned EncryptionKey for Coordinator A, to store after receiving signature from
   * Coordinator B. Performs key split encryption for Coordinator A.
   */
  private static EncryptionKey createCoordinatorAKey(
      ByteString keySplit, EncryptionKey key, Aead keyEncryptionKeyAead, String keyEncryptionKeyUri)
      throws GeneralSecurityException, IOException {
    String encryptedKeySplitA =
        Base64.getEncoder()
            .encodeToString(keyEncryptionKeyAead.encrypt(keySplit.toByteArray(), new byte[0]));
    return key.toBuilder()
        .setJsonEncodedKeyset(encryptedKeySplitA)
        .setKeyEncryptionKeyUri(keyEncryptionKeyUri)
        .build();
  }

  /**
   * Create unsigned EncryptionKey for Coordinator B, to send to Coordinator B in the Create Key
   * service.
   */
  private static EncryptionKey createCoordinatorBKey(EncryptionKey key)
      throws GeneralSecurityException, IOException {
    return key.toBuilder().setJsonEncodedKeyset("").setKeyEncryptionKeyUri("").build();
  }

  /**
   * A Builder pre-populated with shared fields between both generated encryption keys, and also
   * with a key split data containing Coordinator A's signature of the public key material.
   */
  private static EncryptionKey buildEncryptionKey(
      Instant creationTime,
      int validityInDays,
      int ttlInDays,
      KeysetHandle publicKeysetHandle,
      String keyEncryptionKeyUri,
      Optional<PublicKeySign> signatureKey)
      throws ServiceException, IOException {
    String keyId = UUID.randomUUID().toString();
    Instant expiration = creationTime.plus(validityInDays, ChronoUnit.DAYS);
    Instant ttlSec = creationTime.plus(ttlInDays, ChronoUnit.DAYS);

    String publicKeyMaterial = PublicKeyConversionUtil.getPublicKey(publicKeysetHandle);
    EncryptionKey unsignedEncryptionKey =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey(toJsonCleartext(publicKeysetHandle))
            .setPublicKeyMaterial(publicKeyMaterial)
            .setStatus(EncryptionKeyStatus.ACTIVE)
            .setCreationTime(creationTime.toEpochMilli())
            .setExpirationTime(expiration.toEpochMilli())
            // TTL Time must be in seconds due to DynamoDB TTL requirements
            .setTtlTime(ttlSec.getEpochSecond())
            .setKeyType(EncryptionKeyType.MULTI_PARTY_HYBRID_EVEN_KEYSPLIT.name())
            .build();
    try {
      return KeySplitDataUtil.addKeySplitData(
          unsignedEncryptionKey, keyEncryptionKeyUri, signatureKey);
    } catch (GeneralSecurityException e) {
      String msg = "Error generating public key signature";
      throw new ServiceException(Code.INTERNAL, "CRYPTO_ERROR", msg, e);
    }
  }

  /**
   * Combine two {@link KeySplitData} lists into a single list. If two {@link KeySplitData} have the
   * same {@code keySplitKeyEncryptionUri}, only keep one arbitrary one.
   */
  private static ImmutableList<KeySplitData> combineKeySplitData(
      List<KeySplitData> keySplitDataA, List<KeySplitData> keySplitDataB) {
    return ImmutableList.of(keySplitDataA, keySplitDataB).stream()
        .flatMap(List::stream)
        .collect(
            toImmutableMap(
                KeySplitData::getKeySplitKeyEncryptionKeyUri,
                keySplitDataItem -> keySplitDataItem,
                /* make an arbitrary choice */ (keySplitData1, keySplitData2) -> keySplitData1))
        .values()
        .stream()
        .collect(toImmutableList());
  }
}
