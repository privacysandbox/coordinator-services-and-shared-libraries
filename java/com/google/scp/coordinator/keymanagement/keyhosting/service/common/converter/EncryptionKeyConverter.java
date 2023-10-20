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

package com.google.scp.coordinator.keymanagement.keyhosting.service.common.converter;

import static java.lang.Math.toIntExact;

import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyTypeProto.EncryptionKeyType;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.KeyDataProto.KeyData;
import com.google.scp.shared.proto.ProtoUtil;
import java.util.Set;

/**
 * Convert {@link EncryptionKey} values between the API model and storage model, with respect to the
 * Get Encryption Key API.
 */
public final class EncryptionKeyConverter {
  private static final String PRIVATE_KEY_RESOURCE_COLLECTION = "encryptionKeys/";

  private EncryptionKeyConverter() {}

  /**
   * Converts key storage service API encryption key to an API EncryptionKey. Only the {@code
   * keyData} owned by the calling coordinator has its {@code keyMaterial} set. Intended to be used
   * by Get Encryption Key API only.
   */
  public static EncryptionKey toApiEncryptionKey(
      com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto
              .EncryptionKey
          encryptionKey) {
    String name = PRIVATE_KEY_RESOURCE_COLLECTION + encryptionKey.getKeyId();

    ImmutableList<KeyData> keyData =
        encryptionKey.getKeySplitDataList().isEmpty()
            ? getSinglePartyKeyData(encryptionKey)
            : getSplitKeyData(encryptionKey);

    var encryptionKeyBuilder =
        EncryptionKey.newBuilder()
            .setName(name)
            .setPublicKeysetHandle(encryptionKey.getPublicKey())
            .setPublicKeyMaterial(encryptionKey.getPublicKeyMaterial())
            .setExpirationTime(encryptionKey.getExpirationTime())
            .setCreationTime(encryptionKey.getCreationTime())
            .setActivationTime(encryptionKey.getActivationTime())
            .setTtlTime(toIntExact(encryptionKey.getTtlTime()))
            .addAllKeyData(keyData);

    Set<String> keyTypeValues = ProtoUtil.getValidEnumValues(EncryptionKeyType.class);
    if (keyTypeValues.contains(encryptionKey.getKeyType())) {
      encryptionKeyBuilder.setEncryptionKeyType(
          EncryptionKeyType.valueOf(encryptionKey.getKeyType()));
    } else if (encryptionKey.getKeyType().isEmpty()) {
      // Default to single key if no key type specified.
      encryptionKeyBuilder.setEncryptionKeyType(EncryptionKeyType.SINGLE_PARTY_HYBRID_KEY);
    } else {
      throw new IllegalArgumentException("Unrecognized KeyType: " + encryptionKey.getKeyType());
    }

    return encryptionKeyBuilder.build();
  }

  /** For keys created with the new API, directly converts the split key data. */
  private static ImmutableList<KeyData> getSplitKeyData(
      com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto
              .EncryptionKey
          encryptionKey) {
    return encryptionKey.getKeySplitDataList().stream()
        .map(
            keySplitData -> {
              var keyDataBuilder =
                  KeyData.newBuilder()
                      .setPublicKeySignature(keySplitData.getPublicKeySignature())
                      .setKeyEncryptionKeyUri(keySplitData.getKeySplitKeyEncryptionKeyUri());
              // Only populate key material for the key data owned by this coordinator.
              if (encryptionKey.getKeyEncryptionKeyUri() != null
                  && encryptionKey
                      .getKeyEncryptionKeyUri()
                      .equals(keySplitData.getKeySplitKeyEncryptionKeyUri())) {
                keyDataBuilder.setKeyMaterial(encryptionKey.getJsonEncodedKeyset());
              }
              return keyDataBuilder.build();
            })
        .collect(ImmutableList.toImmutableList());
  }

  /** Constructs key data for legacy keys. */
  private static ImmutableList<KeyData> getSinglePartyKeyData(
      com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto
              .EncryptionKey
          encryptionKey) {
    var keyData =
        KeyData.newBuilder()
            .setKeyEncryptionKeyUri(encryptionKey.getKeyEncryptionKeyUri())
            .setKeyMaterial(encryptionKey.getJsonEncodedKeyset())
            .build();
    return ImmutableList.of(keyData);
  }
}
