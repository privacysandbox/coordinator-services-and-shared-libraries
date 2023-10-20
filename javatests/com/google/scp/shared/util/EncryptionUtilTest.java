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

import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeyTemplates;
import com.google.crypto.tink.KeysetHandle;
import com.google.protobuf.ByteString;
import java.io.IOException;
import java.security.GeneralSecurityException;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class EncryptionUtilTest {

  EncryptionUtil target;

  @Before
  public void setUp() {
    target = new EncryptionUtil();
  }

  @Test
  public void generateSymmetricEncryptedKey_encryptsInput()
      throws GeneralSecurityException, IOException {
    KeysetHandle keysetHandle = KeysetHandle.generateNew(KeyTemplates.get("AES128_GCM"));
    Aead encryptionKey = keysetHandle.getPrimitive(Aead.class);
    ByteString encryptedKey = target.generateSymmetricEncryptedKey(encryptionKey);

    Aead decryptedKey =
        KeysetHandleSerializerUtil.fromBinaryCiphertext(encryptedKey, encryptionKey)
            .getPrimitive(Aead.class);

    var payload = "exampleText".getBytes();
    var encryptedPayload = decryptedKey.encrypt(payload, null);
    var decrypted = decryptedKey.decrypt(encryptedPayload, null);

    assertThat(decrypted).isEqualTo(payload);
  }
}
