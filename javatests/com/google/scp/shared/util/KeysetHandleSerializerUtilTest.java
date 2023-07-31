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

package com.google.scp.shared.util;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.fromBinaryCiphertext;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.fromBinaryCleartext;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.fromJsonCiphertext;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.fromJsonCleartext;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toBinaryCiphertext;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toBinaryCleartext;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toJsonCiphertext;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toJsonCleartext;
import static org.junit.Assert.assertThrows;

import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeyTemplates;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.aead.AeadConfig;
import java.security.GeneralSecurityException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class KeysetHandleSerializerUtilTest {

  static {
    try {
      AeadConfig.register();
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Could not register AeadConfig", e);
    }
  }

  @Test
  public void toJsonCleartext_roundTrip() throws Exception {
    var keysetHandle = generateKeysetHandle();

    var cleartext = toJsonCleartext(keysetHandle);
    var recreatedKeysetHandle = fromJsonCleartext(cleartext);

    assertThat(cleartext).isNotEmpty();
    assertEqual(keysetHandle, recreatedKeysetHandle);
  }

  @Test
  public void toJsonCiphertext_roundTrip() throws Exception {
    var keysetHandle = generateKeysetHandle();
    var aead = generateKeyEncryptionKey();

    var ciphertext = toJsonCiphertext(keysetHandle, aead);
    var recreatedKeysetHandle = fromJsonCiphertext(ciphertext, aead);

    assertThat(ciphertext).isNotEmpty();
    assertEqual(keysetHandle, recreatedKeysetHandle);
  }

  @Test
  public void toJsonCiphertext_canDecrypt() throws Exception {
    var keysetHandle = generateKeysetHandle();
    var kek = generateKeyEncryptionKey();
    var payload = "exampleText".getBytes();
    var encrypter = keysetHandle.getPrimitive(Aead.class);
    var encryptedPayload = encrypter.encrypt(payload, null);

    var recreatedKeysetHandle = fromJsonCiphertext(toJsonCiphertext(keysetHandle, kek), kek);
    var decrypter = recreatedKeysetHandle.getPrimitive(Aead.class);

    assertThat(decrypter.decrypt(encryptedPayload, null)).isEqualTo(payload);
  }

  @Test
  public void toBinaryCleartext_roundTrip() throws Exception {
    var keysetHandle = generateKeysetHandle();

    var cleartext = toBinaryCleartext(keysetHandle);
    var recreatedKeysetHandle = fromBinaryCleartext(cleartext);

    assertThat(cleartext).isNotEmpty();
    assertEqual(keysetHandle, recreatedKeysetHandle);
  }

  @Test
  public void toBinaryCiphertext_roundTrip() throws Exception {
    var keysetHandle = generateKeysetHandle();
    var aead = generateKeyEncryptionKey();

    var ciphertext = toBinaryCiphertext(keysetHandle, aead);
    var recreatedKeysetHandle = fromBinaryCiphertext(ciphertext, aead);

    assertThat(ciphertext).isNotEmpty();
    assertEqual(keysetHandle, recreatedKeysetHandle);
  }

  @Test
  public void toBinaryCiphertext_canDecrypt() throws Exception {
    var keysetHandle = generateKeysetHandle();
    var kek = generateKeyEncryptionKey();
    var payload = "exampleText".getBytes();
    var encrypter = keysetHandle.getPrimitive(Aead.class);
    var encryptedPayload = encrypter.encrypt(payload, null);

    var recreatedKeysetHandle = fromBinaryCiphertext(toBinaryCiphertext(keysetHandle, kek), kek);
    var decrypter = recreatedKeysetHandle.getPrimitive(Aead.class);

    assertThat(decrypter.decrypt(encryptedPayload, null)).isEqualTo(payload);
    // attempting to decrypt with a different key should fail.
    assertThrows(GeneralSecurityException.class, () -> kek.decrypt(encryptedPayload, null));
  }

  /** Returns a throwaway AEAD KeysetHandle. */
  private static KeysetHandle generateKeysetHandle() throws GeneralSecurityException {
    return KeysetHandle.generateNew(KeyTemplates.get("AES128_GCM"));
  }

  /** Returns a new Aead key that can be used for encrypting keys. */
  private static Aead generateKeyEncryptionKey() throws GeneralSecurityException {
    return generateKeysetHandle().getPrimitive(Aead.class);
  }

  private static void assertEqual(KeysetHandle a, KeysetHandle b) {
    // KeysetHandles aren't directly comparable and (by design) makes it difficult to access the
    // underlying key material. Assume keys are equal if their type_url and randomly-generated
    // key_id are equal.
    assertThat(a.getKeysetInfo()).isEqualTo(b.getKeysetInfo());
  }
}
