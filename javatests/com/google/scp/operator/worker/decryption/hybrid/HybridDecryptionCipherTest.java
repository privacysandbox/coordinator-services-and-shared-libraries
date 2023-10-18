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

package com.google.scp.operator.worker.decryption.hybrid;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.operator.worker.decryption.hybrid.HybridDecryptionCipher.CONTEXT_INFO;
import static java.nio.charset.StandardCharsets.UTF_8;

import com.google.common.base.Strings;
import com.google.common.io.ByteSource;
import com.google.crypto.tink.HybridDecrypt;
import com.google.crypto.tink.HybridEncrypt;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.hybrid.EciesAeadHkdfPrivateKeyManager;
import com.google.crypto.tink.hybrid.HybridConfig;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class HybridDecryptionCipherTest {

  private KeysetHandle keysetHandle;

  private HybridDecryptionCipher hybridDecryptionCipher;

  @Before
  public void setUp() throws Exception {
    HybridConfig.register(); // Config must be registered before calling encryption or decryption
    keysetHandle =
        KeysetHandle.generateNew(
            EciesAeadHkdfPrivateKeyManager.rawEciesP256HkdfHmacSha256Aes128GcmCompressedTemplate());
    hybridDecryptionCipher =
        HybridDecryptionCipher.of(keysetHandle.getPrimitive(HybridDecrypt.class));
  }

  @Test
  public void decryptionTest() throws Exception {
    // Use a longer string to test encrypting larger payloads
    String message = Strings.repeat("This is a secret", 10000);
    byte[] encryptedPayload = hybridEncryptData(message.getBytes(UTF_8));

    ByteSource decryptedPayload = hybridDecryptionCipher.decrypt(ByteSource.wrap(encryptedPayload));

    String decryptedMessage = new String(decryptedPayload.read(), UTF_8);
    assertThat(decryptedMessage).isEqualTo(message);
  }

  private byte[] hybridEncryptData(byte[] plaintextPayload) throws Exception {
    HybridEncrypt hybridEncrypt =
        keysetHandle.getPublicKeysetHandle().getPrimitive(HybridEncrypt.class);
    return hybridEncrypt.encrypt(plaintextPayload, CONTEXT_INFO);
  }
}
