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

import static com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.KeyGenerationUtil.countExistingValidKeys;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toJsonCiphertext;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toJsonCleartext;

import com.google.common.collect.ImmutableList;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KeyEncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.Annotations.KmsKeyAead;
import com.google.scp.coordinator.keymanagement.shared.dao.common.KeyDb;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.util.KeyParams;
import com.google.scp.shared.util.PublicKeyConversionUtil;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.UUID;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Generates keys and stores in datastore.
 *
 * @deprecated Single-party key features are deprecated. Pending removal b/282204533.
 */
@Deprecated
public class CreateKeysTask {

  private static final Logger LOGGER = LoggerFactory.getLogger(CreateKeysTask.class);
  private final KeyDb keyDb;
  private final Aead kmsKeyAead;
  private final String keyEncryptionKeyUri;
  /**
   * Amount of time a key must be valid for to not be refreshed. Keys that expire before (now +
   * keyRefreshWindow) should be replaced with a new key.
   */
  private static final Duration keyRefreshWindow = Duration.ofDays(1);

  static {
    try {
      HybridConfig.register();
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Error initializing tink.");
    }
  }

  @Inject
  public CreateKeysTask(
      KeyDb keyDb, @KmsKeyAead Aead kmsKeyAead, @KeyEncryptionKeyUri String keyEncryptionKeyUri) {
    this.keyDb = keyDb;
    this.kmsKeyAead = kmsKeyAead;
    this.keyEncryptionKeyUri = keyEncryptionKeyUri;
  }

  /**
   * Creates and stores in datastore count number of keys for hybrid encryption which expires after
   * validityInDays days and has a Time-to-Live of ttlInDays.
   *
   * <p>For key regeneration {@link CreateKeysTask#replaceExpiringKeys(int, int, int)} should be
   * used.
   *
   * @deprecated Single-party key features are deprecated. Pending removal b/282204533.
   */
  @Deprecated
  public void createKeys(int count, int validityInDays, int ttlInDays) throws ServiceException {
    ImmutableList<EncryptionKey> keys = generateKeys(count, validityInDays, ttlInDays);
    keyDb.createKeys(keys);
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
    var numExistingKeys = countExistingValidKeys(keyDb, keyRefreshWindow, numDesiredKeys);
    var numKeysToCreate = numDesiredKeys - numExistingKeys;

    if (numKeysToCreate <= 0) {
      LOGGER.info(String.format("Found %d valid keys, skipping key creation", numExistingKeys));
      return;
    }

    createKeys(numKeysToCreate, validityInDays, ttlInDays);
  }

  private ImmutableList<EncryptionKey> generateKeys(int count, int validityInDays, int ttlInDays)
      throws ServiceException {
    if (count == 0) {
      return ImmutableList.of();
    }
    Instant now = Instant.now();
    Instant expiration = now.plus(validityInDays, ChronoUnit.DAYS);
    Instant ttlSec = now.plus(ttlInDays, ChronoUnit.DAYS);
    ImmutableList.Builder<EncryptionKey> builder = ImmutableList.builder();

    try {
      var keyTemplate = KeyParams.getDefaultKeyTemplate();

      for (int i = 0; i < count; i++) {
        KeysetHandle privateKeysetHandle = KeysetHandle.generateNew(keyTemplate);
        KeysetHandle publicKeysetHandle = privateKeysetHandle.getPublicKeysetHandle();
        builder.add(
            EncryptionKey.newBuilder()
                .setKeyId(UUID.randomUUID().toString())
                .setPublicKey(toJsonCleartext(publicKeysetHandle))
                .setPublicKeyMaterial(PublicKeyConversionUtil.getPublicKey(publicKeysetHandle))
                .setJsonEncodedKeyset(toJsonCiphertext(privateKeysetHandle, kmsKeyAead))
                .setKeyEncryptionKeyUri(keyEncryptionKeyUri)
                .setCreationTime(now.toEpochMilli())
                .setExpirationTime(expiration.toEpochMilli())
                // TTL Time must be in seconds due to DynamoDB TTL requirements
                .setTtlTime(ttlSec.getEpochSecond())
                .build());
      }
    } catch (GeneralSecurityException | IOException e) {
      String msg = "Error generating keys.";
      LOGGER.error(msg, e);
      throw new ServiceException(Code.INTERNAL, "CRYPTO_ERROR", msg, e);
    }
    LOGGER.info(String.format("Successfully generated %s keys", count));
    return builder.build();
  }
}
