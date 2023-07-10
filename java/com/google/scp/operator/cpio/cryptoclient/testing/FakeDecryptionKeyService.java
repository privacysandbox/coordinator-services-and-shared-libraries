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

package com.google.scp.operator.cpio.cryptoclient.testing;

import com.google.common.io.ByteSource;
import com.google.crypto.tink.HybridDecrypt;
import com.google.crypto.tink.HybridEncrypt;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.hybrid.EciesAeadHkdfPrivateKeyManager;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService;
import com.google.scp.operator.cpio.cryptoclient.model.ErrorReason;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.HashMap;
import java.util.Map;

/**
 * Fake {@code DecryptionKeyService} that returns hybrid decryption keys. Keys returned by getKey
 * will be consistent between multiple invocations for the same key id (tied to the lifespan of the
 * service). Can be set to throw exceptions and saves the last decryptionKeyId it was called with.
 */
public final class FakeDecryptionKeyService implements DecryptionKeyService {

  private boolean shouldThrow;
  private String lastDecryptionKeyIdUsed;
  private Map<String, KeysetHandle> keyMap = new HashMap<String, KeysetHandle>();

  /** Creates a new instance of the {@code FakeDecryptionKeyService} class. */
  public FakeDecryptionKeyService() {
    this.shouldThrow = false;
  }

  @Override
  public HybridDecrypt getDecrypter(String decryptionKeyId) throws KeyFetchException {
    if (shouldThrow) {
      throw new KeyFetchException(
          new IllegalStateException("FakeDecryptionKeyService was set to throw"),
          ErrorReason.UNKNOWN_ERROR);
    }

    lastDecryptionKeyIdUsed = decryptionKeyId;

    try {
      return getKeysetHandle(decryptionKeyId).getPrimitive(HybridDecrypt.class);
    } catch (GeneralSecurityException e) {
      throw new IllegalStateException("Unexpected key generation error", e);
    }
  }

  /**
   * Set if the service should throw a {@code KeyFetchException} when calling the {@code
   * getDecrypter} method.
   */
  public void setShouldThrow(boolean shouldThrow) {
    this.shouldThrow = shouldThrow;
  }

  /** Returns the last decryption key ID used in the {@code getDecrypter} method. */
  public String getLastDecryptionKeyIdUsed() {
    return lastDecryptionKeyIdUsed;
  }

  /** Returns the keyset handle with the specified key id, generating a new key if necessary. */
  public KeysetHandle getKeysetHandle(String decryptionKeyId) {
    return keyMap.computeIfAbsent(decryptionKeyId, (k) -> createKey());
  }

  /** Generates a new hybrid decryption key. */
  private static KeysetHandle createKey() {
    try {
      HybridConfig.register();
      return KeysetHandle.generateNew(
          EciesAeadHkdfPrivateKeyManager.rawEciesP256HkdfHmacSha256Aes128GcmCompressedTemplate());
    } catch (GeneralSecurityException e) {
      throw new IllegalStateException("Failed to create fake key", e);
    }
  }

  /** Helper function for generating ciphertext encrypted with the specified key id. */
  public ByteSource generateCiphertext(String keyId, ByteSource plaintext)
      throws KeyFetchException, GeneralSecurityException, IOException {
    var keysetHandle = getKeysetHandle(keyId).getPublicKeysetHandle();

    var hybridEncrypt = keysetHandle.getPrimitive(HybridEncrypt.class);
    return ByteSource.wrap(hybridEncrypt.encrypt(plaintext.read(), null));
  }
}
