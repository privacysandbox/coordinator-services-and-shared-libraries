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

package com.google.scp.coordinator.keymanagement.shared.util;

import static com.google.common.collect.ImmutableMap.toImmutableMap;

import com.google.common.base.Joiner;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.crypto.tink.PublicKeySign;
import com.google.crypto.tink.PublicKeyVerify;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.KeySplitDataProto.KeySplitData;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.time.Instant;
import java.util.Base64;
import java.util.Map;
import java.util.Optional;

/**
 * Utility methods for signing public key material for an encryption key and assigning them to new
 * KeySplitData
 */
public final class KeySplitDataUtil {
  private KeySplitDataUtil() {}

  /**
   * Adds the a key split data containing the speicified encryption key uri and signature if
   * signature key is provided. Naively appends to the list, without consideration for possible
   * duplicates.
   */
  public static EncryptionKey addKeySplitData(
      EncryptionKey encryptionKey, String keyEncryptionKeyUri, Optional<PublicKeySign> signatureKey)
      throws GeneralSecurityException {
    ImmutableList<KeySplitData> keySplitDataList =
        ImmutableList.<KeySplitData>builder()
            .addAll(encryptionKey.getKeySplitDataList())
            .add(buildKeySplitData(encryptionKey, keyEncryptionKeyUri, signatureKey))
            .build();
    return encryptionKey.toBuilder()
        .clearKeySplitData()
        .addAllKeySplitData(keySplitDataList)
        .build();
  }

  /**
   * Given a map of coordinators (named by their key encryption key uri) to a {@code
   * PublicKeyVerify} that can verify against their signature, verifies that a proper signature for
   * each coordinator exists in the {@link KeySplitData} of the {@link EncryptionKey}.
   */
  public static void verifyEncryptionKeySignatures(
      EncryptionKey encryptionKey, ImmutableMap<String, PublicKeyVerify> publicKeyVerifiers)
      throws GeneralSecurityException {
    byte[] message = encryptionKeySignatureMessage(encryptionKey).getBytes();
    ImmutableMap<String, KeySplitData> keySplitDataMap =
        encryptionKey.getKeySplitDataList().stream()
            .collect(
                toImmutableMap(
                    KeySplitData::getKeySplitKeyEncryptionKeyUri,
                    keySplitDataItem -> keySplitDataItem));
    for (Map.Entry<String, PublicKeyVerify> publicKeyVerify : publicKeyVerifiers.entrySet()) {
      Optional<KeySplitData> keySplitData =
          Optional.ofNullable(keySplitDataMap.get(publicKeyVerify.getKey()));
      if (keySplitData.isPresent()) {
        byte[] signature = Base64.getDecoder().decode(keySplitData.get().getPublicKeySignature());
        try {
          publicKeyVerify.getValue().verify(signature, message);
        } catch (GeneralSecurityException e) {
          String err =
              String.format(
                  "Signature of coordinator %s failed to verify",
                  keySplitData.get().getKeySplitKeyEncryptionKeyUri());
          throw new GeneralSecurityException(err, e);
        }
      } else {
        String notFoundErr =
            String.format(
                "Signature for coordinator %s not found in KeySplitData list",
                publicKeyVerify.getKey());
        throw new GeneralSecurityException(notFoundErr);
      }
    }
  }

  private static KeySplitData buildKeySplitData(
      EncryptionKey encryptionKey, String keyEncryptionKeyUri, Optional<PublicKeySign> signatureKey)
      throws GeneralSecurityException {
    KeySplitData.Builder keySplitData =
        KeySplitData.newBuilder().setKeySplitKeyEncryptionKeyUri(keyEncryptionKeyUri);
    if (signatureKey.isPresent()) {
      keySplitData.setPublicKeySignature(
          generateEncryptionKeySignature(encryptionKey, signatureKey.get()));
    }
    return keySplitData.build();
  }

  /**
   * The signature of an EncryptionKey material in ${key_id}:${creation_time}:${public_key_material}
   * format.
   */
  private static String generateEncryptionKeySignature(
      EncryptionKey encryptionKey, PublicKeySign signatureKey) throws GeneralSecurityException {
    return new String(
        Base64.getEncoder()
            .encodeToString(
                signatureKey.sign(
                    encryptionKeySignatureMessage(encryptionKey)
                        .getBytes(StandardCharsets.UTF_8))));
  }

  /**
   * The signature of an EncryptionKey material in ${key_id}|${iso 8601
   * creation_time}|${public_key_material} format.
   */
  private static String encryptionKeySignatureMessage(EncryptionKey encryptionKey) {
    // ${key_id}|${iso 8601 creation_time}|${public_key_material}
    Instant creationTime = Instant.ofEpochMilli(encryptionKey.getCreationTime());
    return Joiner.on("|")
        .join(
            encryptionKey.getKeyId(),
            creationTime.toString(),
            encryptionKey.getPublicKeyMaterial());
  }
}
