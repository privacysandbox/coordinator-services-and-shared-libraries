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
import static com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.KeyGenerationUtil.findTemporaryKeys;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toJsonCleartext;

import com.google.common.collect.ImmutableList;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.PublicKeySign;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.protobuf.ByteString;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.KeyStorageClient;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.keyid.KeyIdFactory;
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
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Base64;
import java.util.List;
import java.util.Optional;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Shared base class between cloud providers for Split Key Generation. Handles generating the key
 * split, sending the second split to Coordinator B, and receiving and storing the signed response.
 */
public abstract class CreateSplitKeyTaskBase implements CreateSplitKeyTask {

  private static final Logger LOGGER = LoggerFactory.getLogger(CreateSplitKeyTaskBase.class);

  private static final int KEY_SERVICE_MAX_RETRY = 5;

  protected final Aead keyEncryptionKeyAead;
  protected final String keyEncryptionKeyUri;
  protected final Optional<PublicKeySign> signatureKey;
  protected final KeyDb keyDb;
  protected final KeyStorageClient keyStorageClient;
  protected final KeyIdFactory keyIdFactory;

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
      KeyStorageClient keyStorageClient,
      KeyIdFactory keyIdFactory) {
    this.keyEncryptionKeyAead = keyEncryptionKeyAead;
    this.keyEncryptionKeyUri = keyEncryptionKeyUri;
    this.signatureKey = signatureKey;
    this.keyDb = keyDb;
    this.keyStorageClient = keyStorageClient;
    this.keyIdFactory = keyIdFactory;
  }

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

    createSplitKey(numKeysToCreate, validityInDays, ttlInDays, Instant.now());
  }

  public void create(int numDesiredKeys, int validityInDays, int ttlInDays)
      throws ServiceException {
    // Anchor one consistent definition of now for the creation process.
    Instant now = Instant.now();

    // Check if there are enough number of active keys, if not, create any missing keys.
    ImmutableList<EncryptionKey> activeKeys = keyDb.getActiveKeys(numDesiredKeys, now);
    LOGGER.info("Number of active keys: " + activeKeys.size());

    if (activeKeys.size() < numDesiredKeys) {
      createSplitKey(numDesiredKeys - activeKeys.size(), validityInDays, ttlInDays, now);
    }
    activeKeys = keyDb.getActiveKeys(numDesiredKeys, now);
    if (activeKeys.size() < numDesiredKeys) {
      throw new AssertionError("Enough keys should have been created to meet the desired number.");
    }

    // Check if there will be enough number of active keys when each active key expires, if not,
    // create any missing pending-active keys.
    ImmutableList<Instant> expirations =
        activeKeys.stream()
            .map(EncryptionKey::getExpirationTime)
            .map(Instant::ofEpochMilli)
            .distinct()
            .sorted()
            .collect(toImmutableList());

    for (Instant expiration : expirations) {
      int actual = keyDb.getActiveKeys(numDesiredKeys, expiration).size();
      if (actual < numDesiredKeys) {
        createSplitKey(
            numDesiredKeys - actual,
            validityInDays,
            ttlInDays,
            expiration.minus(KEY_REFRESH_WINDOW));
      }
      actual = keyDb.getActiveKeys(numDesiredKeys, expiration).size();
      if (actual < numDesiredKeys) {
        throw new AssertionError(
            "Enough pending-active keys should have been created to meet the desired number.");
      }
    }
  }

  /**
   * Key generation process. Performs encryption key generation and splitting, key storage request,
   * and database persistence with signatures. Coordinator B encryption and key storage creation are
   * handled by abstract methods implemented in each cloud provider.
   *
   * @param activation the instant when the key should be active for encryption.
   * @param dataKey Passed to encryptPeerCoordinatorSplit and sendKeySplitToPeerCoordinator as
   *     needed for each cloud provider.
   */
  protected final void createSplitKeyBase(
      int count, int validityInDays, int ttlInDays, Instant activation, Optional<DataKey> dataKey)
      throws ServiceException {
    LOGGER.info(String.format("Trying to generate %d keys.", count));
    for (int i = 0; i < count; i++) {
      createSplitKeyBase(validityInDays, ttlInDays, activation, dataKey, 0);
    }
    LOGGER.info(
        String.format("Successfully generated %s keys to be active on %s.", count, activation));
  }

  private void createSplitKeyBase(
      int validityInDays,
      int ttlInDays,
      Instant activation,
      Optional<DataKey> dataKey,
      int keyServiceRetryCount)
      throws ServiceException {
    try {
      // Remove all placeholder keys in keyDb first before creating keys.
      removePlaceholderKeys(keyDb);
    } catch (ServiceException e) {
      LOGGER.warn("Error when removing placeholder keys.\n" + e.getErrorReason());
      if (keyServiceRetryCount < KEY_SERVICE_MAX_RETRY) {
        LOGGER.info(
            "Retry removing placeholder keys. Current retry count: " + keyServiceRetryCount);
        createSplitKeyBase(
            validityInDays, ttlInDays, activation, dataKey, keyServiceRetryCount + 1);
        return;
      }
      LOGGER.error("Retries exhausted when removing placeholder keys.");
      throw e;
    }

    Instant creationTime = Instant.now();
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
              keyIdFactory.getNextKeyId(keyDb),
              creationTime,
              activation,
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
    } catch (GeneralSecurityException | IOException | ServiceException e) {
      String msg = "Error generating keys.";
      LOGGER.warn(msg + "\n" + e.getMessage());
      if (keyServiceRetryCount < KEY_SERVICE_MAX_RETRY) {
        LOGGER.info("Retry generating keys. Current retry count: " + keyServiceRetryCount);
        createSplitKeyBase(
            validityInDays, ttlInDays, activation, dataKey, keyServiceRetryCount + 1);
        return;
      }
      LOGGER.error("Retries exhausted when generating keys.");
      throw new ServiceException(Code.INTERNAL, "CRYPTO_ERROR", msg, e);
    }

    try {
      // Reserve the key ID with a placeholder key that's not valid yet. Will be made valid at a
      // later step once key-split is successfully delivered to coordinator B.
      LOGGER.info("Generating the placeholder key: " + unsignedCoordinatorAKey.getKeyId());
      keyDb.createKey(
          unsignedCoordinatorAKey.toBuilder()
              // Setting activation_time to expiration_time such that the key is invalid for now.
              .setActivationTime(unsignedCoordinatorAKey.getExpirationTime())
              .build(),
          false);
    } catch (ServiceException e) {
      LOGGER.warn("Failed to insert placeholder key due to database error: " + e.getErrorReason());
      if (keyServiceRetryCount < KEY_SERVICE_MAX_RETRY) {
        LOGGER.info("Retry creating key. Current retry count: " + keyServiceRetryCount);
        createSplitKeyBase(
            validityInDays, ttlInDays, activation, dataKey, keyServiceRetryCount + 1);
        return;
      }
      LOGGER.error("Retries exhausted when inserting placeholder key.");
      throw e;
    }
    // Send Coordinator B valid key split
    try {

      EncryptionKey partyBResponse =
          sendKeySplitToPeerCoordinator(unsignedCoordinatorBKey, encryptedKeySplitB, dataKey);
      LOGGER.info(
          "Sent key " + unsignedCoordinatorBKey.getKeyId() + " to peer coordinator successfully.");

      // Accumulate signatures and store to KeyDB
      String persistedKeyId =
          signAndPersistKey(keyDb, unsignedCoordinatorAKey, partyBResponse, true);
      LOGGER.info(
          String.format(
              "Updated placeholder key %s to persistent key successfully.", persistedKeyId));
    } catch (ServiceException e) {
      LOGGER.warn(
          "Failed to send key in peer coordinator or update placeholder key to persistent key due"
              + " to: "
              + e.getErrorReason());
      LOGGER.info("Deleting the placeholder key: " + unsignedCoordinatorAKey.getKeyId());
      keyDb.deleteKey(unsignedCoordinatorAKey.getKeyId());

      if (keyServiceRetryCount < KEY_SERVICE_MAX_RETRY) {
        LOGGER.info("Retry sending/creating key. Current retry count: " + keyServiceRetryCount);
        createSplitKeyBase(
            validityInDays, ttlInDays, activation, dataKey, keyServiceRetryCount + 1);
        return;
      }
      LOGGER.error("Retries exhausted when sending key to peer coordinator.");
      throw e;
    }
  }

  /**
   * GCP key generation with key sync to AWS. Performs encryption key generation and splitting, key
   * storage request, and database persistence with signatures to both GCP and AWS Key Database.
   * Coordinator B encryption and key storage creation are handled by abstract methods implemented
   * in each cloud provider.
   *
   * <p>NOTE: The following method is of limited time use during the AWS cross-cloud coordinator
   * migration.
   *
   * @param activation the instant when the key should be active for encryption.
   * @param dataKey Passed to encryptPeerCoordinatorSplit and sendKeySplitToPeerCoordinator as
   *     needed for each cloud provider.
   * @param keySyncKmsUri AWS KMS URI used to wrap private key as part of key sync.
   * @param keySyncAead AWS KMS AEAD used to wrap private key as part of key sync.
   * @param keySyncDb AWS DynamoDB instance used to persist key as part of key sync.
   */
  protected final void createSplitKeyWithAwsKeySyncBase(
      int count,
      int validityInDays,
      int ttlInDays,
      Instant activation,
      Optional<DataKey> dataKey,
      String keySyncKmsUri,
      Aead keySyncAead,
      KeyDb keySyncDb)
      throws ServiceException {
    LOGGER.info("Running under key sync mode..");
    LOGGER.info(String.format("Trying to generate %d keys.", count));
    for (int i = 0; i < count; i++) {
      createSplitKeyWithAwsKeySyncBase(
          validityInDays, ttlInDays, activation, dataKey, keySyncKmsUri, keySyncAead, keySyncDb, 0);
    }
    LOGGER.info(
        String.format("Successfully generated %s keys to be active on %s.", count, activation));
  }

  private void createSplitKeyWithAwsKeySyncBase(
      int validityInDays,
      int ttlInDays,
      Instant activation,
      Optional<DataKey> dataKey,
      String keySyncKmsUri,
      Aead keySyncAead,
      KeyDb keySyncDb,
      int keyServiceRetryCount)
      throws ServiceException {
    // 1. Try remove all placeholder keys in keyDb first before creating keys.
    try {
      removePlaceholderKeys(keyDb);
    } catch (ServiceException e) {
      LOGGER.warn("Error when removing placeholder keys.\n" + e.getErrorReason());
      if (keyServiceRetryCount < KEY_SERVICE_MAX_RETRY) {
        LOGGER.info(
            "Retry removing placeholder keys. Current retry count: " + keyServiceRetryCount);
        createSplitKeyWithAwsKeySyncBase(
            validityInDays,
            ttlInDays,
            activation,
            dataKey,
            keySyncKmsUri,
            keySyncAead,
            keySyncDb,
            keyServiceRetryCount + 1);
        return;
      }
      LOGGER.error("Retries exhausted when removing placeholder keys.");
      throw e;
    }

    // 2. Generate the key splits and encrypt the splits using both GCP and AWS KMS.
    Instant creationTime = Instant.now();
    EncryptionKey unsignedCoordinatorAKey;
    EncryptionKey unsignedAwsCoordinatorAKey;
    EncryptionKey unsignedCoordinatorBKey;
    String encryptedKeySplitB;
    try {
      var keyTemplate = KeyParams.getDefaultKeyTemplate();
      KeysetHandle privateKeysetHandle = KeysetHandle.generateNew(keyTemplate);
      KeysetHandle publicKeysetHandle = privateKeysetHandle.getPublicKeysetHandle();

      ImmutableList<ByteString> keySplits = KeySplitUtil.xorSplit(privateKeysetHandle, 2);
      String nextKeyId = keyIdFactory.getNextKeyId(keyDb);
      EncryptionKey key =
          buildEncryptionKey(
              nextKeyId,
              creationTime,
              activation,
              validityInDays,
              ttlInDays,
              publicKeysetHandle,
              keyEncryptionKeyUri,
              signatureKey);
      unsignedCoordinatorAKey =
          createCoordinatorAKey(keySplits.get(0), key, keyEncryptionKeyAead, keyEncryptionKeyUri);
      EncryptionKey awsKey =
          buildEncryptionKey(
              nextKeyId,
              creationTime,
              activation,
              validityInDays,
              ttlInDays,
              publicKeysetHandle,
              keySyncKmsUri,
              signatureKey);
      unsignedAwsCoordinatorAKey =
          createCoordinatorAKey(keySplits.get(0), awsKey, keySyncAead, keySyncKmsUri);
      unsignedCoordinatorBKey =
          KeySplitDataUtil.addKeySplitData(createCoordinatorBKey(key), keySyncKmsUri, signatureKey);

      encryptedKeySplitB =
          encryptPeerCoordinatorSplit(keySplits.get(1), dataKey, key.getPublicKeyMaterial());
    } catch (GeneralSecurityException | IOException | ServiceException e) {
      String msg = "Error generating keys.";
      LOGGER.warn(msg + "\n" + e.getMessage());
      if (keyServiceRetryCount < KEY_SERVICE_MAX_RETRY) {
        LOGGER.info("Retry generating keys. Current retry count: " + keyServiceRetryCount);
        createSplitKeyWithAwsKeySyncBase(
            validityInDays,
            ttlInDays,
            activation,
            dataKey,
            keySyncKmsUri,
            keySyncAead,
            keySyncDb,
            keyServiceRetryCount + 1);
        return;
      }
      LOGGER.error("Retries exhausted when generating keys.");
      throw new ServiceException(Code.INTERNAL, "CRYPTO_ERROR", msg, e);
    }

    // 3. Reserve placeholder entry in the GCP keyDB of the cross-cloud coordinators.
    try {
      // Reserve the key ID with a placeholder key that's not valid yet. Will be made valid at a
      // later step once key-split is successfully delivered to coordinator B.
      LOGGER.info("Generating the placeholder key: " + unsignedCoordinatorAKey.getKeyId());
      keyDb.createKey(
          unsignedCoordinatorAKey.toBuilder()
              // Setting activation_time to expiration_time such that the key is invalid for now.
              .setActivationTime(unsignedCoordinatorAKey.getExpirationTime())
              .build(),
          false);
    } catch (ServiceException e) {
      LOGGER.warn("Failed to insert placeholder key due to database error: " + e.getErrorReason());
      if (keyServiceRetryCount < KEY_SERVICE_MAX_RETRY) {
        LOGGER.info("Retry creating key. Current retry count: " + keyServiceRetryCount);
        createSplitKeyWithAwsKeySyncBase(
            validityInDays,
            ttlInDays,
            activation,
            dataKey,
            keySyncKmsUri,
            keySyncAead,
            keySyncDb,
            keyServiceRetryCount + 1);
        return;
      }
      LOGGER.error("Retries exhausted when inserting placeholder key.");
      throw e;
    }

    // 4. Send valid key split to Coordinator B create key service.
    EncryptionKey partyBResponse;
    try {
      partyBResponse =
          sendKeySplitToPeerCoordinator(unsignedCoordinatorBKey, encryptedKeySplitB, dataKey);
      LOGGER.info(
          "Sent key " + unsignedCoordinatorBKey.getKeyId() + " to peer coordinator successfully.");
    } catch (ServiceException e) {
      LOGGER.warn("Failed to send key to peer coordinator due to: " + e.getErrorReason());
      LOGGER.info("Deleting the placeholder key: " + unsignedCoordinatorAKey.getKeyId());
      keyDb.deleteKey(unsignedCoordinatorAKey.getKeyId());

      if (keyServiceRetryCount < KEY_SERVICE_MAX_RETRY) {
        LOGGER.info("Retry sending/creating key. Current retry count: " + keyServiceRetryCount);
        createSplitKeyWithAwsKeySyncBase(
            validityInDays,
            ttlInDays,
            activation,
            dataKey,
            keySyncKmsUri,
            keySyncAead,
            keySyncDb,
            keyServiceRetryCount + 1);
        return;
      }
      LOGGER.error("Retries exhausted when sending key to peer coordinator.");
      throw e;
    }

    // 5. Encrypt and sign key split A with the AWS key sync KMS key and store to DynamoDB
    try {
      String peerCoordinatorKeySyncKmsUri =
          partyBResponse.getKeySplitData(1).getKeySplitKeyEncryptionKeyUri();
      String persistedKeyId =
          signAndPersistKey(
              keySyncDb,
              updateKeySplitDataWithKeyURI(unsignedAwsCoordinatorAKey, keySyncKmsUri, signatureKey),
              updateKeySplitDataWithKeyURI(
                  partyBResponse, peerCoordinatorKeySyncKmsUri, signatureKey),
              false);
      LOGGER.info(
          String.format("Successfully persisted key to Type-A DynamoDB KeyDB: %s", persistedKeyId));
    } catch (ServiceException | GeneralSecurityException e) {
      LOGGER.warn("Failed to persist key to Type-A DynamoDB KeyDB: " + e.getMessage());
      LOGGER.info("Deleting the placeholder key: " + unsignedCoordinatorAKey.getKeyId());
      keyDb.deleteKey(unsignedCoordinatorAKey.getKeyId());

      if (keyServiceRetryCount < KEY_SERVICE_MAX_RETRY) {
        LOGGER.info("Retry sending/creating key. Current retry count: " + keyServiceRetryCount);
        createSplitKeyWithAwsKeySyncBase(
            validityInDays,
            ttlInDays,
            activation,
            dataKey,
            keySyncKmsUri,
            keySyncAead,
            keySyncDb,
            keyServiceRetryCount + 1);
        return;
      }
      LOGGER.error("Retries exhausted when sending key to peer coordinator.");
      throw new ServiceException(Code.UNAVAILABLE, "UNAVAILABLE", e.getMessage(), e);
    }

    // 6. Encrypt and sign split A with the xC GCP KMS, update the placeholder key and finalize key
    // generation.
    try {
      String peerCoordinatorKeyEncryptionKeyUri =
          partyBResponse.getKeySplitData(0).getKeySplitKeyEncryptionKeyUri();
      String persistedKeyId =
          signAndPersistKey(
              keyDb,
              updateKeySplitDataWithKeyURI(
                  unsignedCoordinatorAKey, keyEncryptionKeyUri, signatureKey),
              updateKeySplitDataWithKeyURI(
                  partyBResponse, peerCoordinatorKeyEncryptionKeyUri, signatureKey),
              true);
      LOGGER.info(
          String.format(
              "Updated placeholder key %s to persistent key successfully.", persistedKeyId));
    } catch (ServiceException | GeneralSecurityException e) {
      LOGGER.warn(
          "Failed to update placeholder key during key persistence due to: " + e.getMessage());
      LOGGER.info("Deleting the placeholder key: " + unsignedCoordinatorAKey.getKeyId());
      keyDb.deleteKey(unsignedCoordinatorAKey.getKeyId());

      if (keyServiceRetryCount < KEY_SERVICE_MAX_RETRY) {
        LOGGER.info("Retry sending/creating key. Current retry count: " + keyServiceRetryCount);
        createSplitKeyWithAwsKeySyncBase(
            validityInDays,
            ttlInDays,
            activation,
            dataKey,
            keySyncKmsUri,
            keySyncAead,
            keySyncDb,
            keyServiceRetryCount + 1);
        return;
      }
      LOGGER.error("Retries exhausted when sending key to peer coordinator.");
      throw new ServiceException(Code.UNAVAILABLE, "UNAVAILABLE", e.getMessage(), e);
    }
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
   * A Builder pre-populated with shared fields between both generated encryption keys, and also
   * with a key split data containing Coordinator A's signature of the public key material.
   */
  private static EncryptionKey buildEncryptionKey(
      String keyId,
      Instant creationTime,
      Instant activation,
      int validityInDays,
      int ttlInDays,
      KeysetHandle publicKeysetHandle,
      String keyEncryptionKeyUri,
      Optional<PublicKeySign> signatureKey)
      throws ServiceException, IOException {
    Instant expiration = activation.plus(validityInDays, ChronoUnit.DAYS);
    Instant ttlSec = activation.plus(ttlInDays, ChronoUnit.DAYS);

    String publicKeyMaterial = PublicKeyConversionUtil.getPublicKey(publicKeysetHandle);
    EncryptionKey unsignedEncryptionKey =
        EncryptionKey.newBuilder()
            .setKeyId(keyId)
            .setPublicKey(toJsonCleartext(publicKeysetHandle))
            .setPublicKeyMaterial(publicKeyMaterial)
            .setStatus(EncryptionKeyStatus.ACTIVE)
            .setCreationTime(creationTime.toEpochMilli())
            .setActivationTime(activation.toEpochMilli())
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
   * Create unsigned EncryptionKey for Coordinator B, to send to Coordinator B in the Create Key
   * service.
   */
  private static EncryptionKey createCoordinatorBKey(EncryptionKey key)
      throws GeneralSecurityException, IOException {
    return key.toBuilder().setJsonEncodedKeyset("").setKeyEncryptionKeyUri("").build();
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

  private void removePlaceholderKeys(KeyDb keyDb) throws ServiceException {
    ImmutableList<EncryptionKey> tempKeys = findTemporaryKeys(keyDb);
    LOGGER.info("Number of placeholder keys found in keyDb: " + tempKeys.size());
    for (EncryptionKey tmpKey : tempKeys) {
      LOGGER.info("Removing placeholder key: " + tmpKey.getKeyId());
      keyDb.deleteKey(tmpKey.getKeyId());
    }
    LOGGER.info(
        "Successfully removed all placeholder keys or no placeholder keys were found in Db.");
  }

  private static EncryptionKey updateKeySplitDataWithKeyURI(
      EncryptionKey key, String kmsKeyUri, Optional<PublicKeySign> signatureKey)
      throws GeneralSecurityException {
    return KeySplitDataUtil.addKeySplitData(
        key.toBuilder().clearKeySplitData().build(), kmsKeyUri, signatureKey);
  }

  private static String signAndPersistKey(
      KeyDb keyDb, EncryptionKey unsignedKey, EncryptionKey partyBResponse, boolean overwriteKey)
      throws ServiceException {
    EncryptionKey signedKey = signCoordinatorKeyWithPeerResponse(unsignedKey, partyBResponse);
    keyDb.createKey(signedKey, overwriteKey);
    return signedKey.getKeyId();
  }

  private static EncryptionKey signCoordinatorKeyWithPeerResponse(
      EncryptionKey unsignedKey, EncryptionKey peerResponse) {
    return unsignedKey.toBuilder()
        .clearKeySplitData()
        .addAllKeySplitData(
            combineKeySplitData(
                peerResponse.getKeySplitDataList(), unsignedKey.getKeySplitDataList()))
        .build();
  }
}
