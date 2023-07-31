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

import com.google.common.base.Converter;
import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.EncodedPublicKeyProto.EncodedPublicKey;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey;

/** Converts between ImmutableList of {@link EncryptionKey} and {@link EncodedPublicKey} */
public final class EncodedPublicKeyListConverter
    extends Converter<ImmutableList<EncryptionKey>, ImmutableList<EncodedPublicKey>> {

  @Override
  protected ImmutableList<EncodedPublicKey> doForward(ImmutableList<EncryptionKey> encryptionKeys) {
    return encryptionKeys.stream()
        .map(
            publicKey ->
                EncodedPublicKey.newBuilder()
                    .setId(publicKey.getKeyId())
                    .setKey(publicKey.getPublicKeyMaterial())
                    .build())
        .collect(ImmutableList.toImmutableList());
  }

  @Override
  protected ImmutableList<EncryptionKey> doBackward(
      ImmutableList<EncodedPublicKey> encodedPublicKeys) {
    throw new UnsupportedOperationException();
  }
}
