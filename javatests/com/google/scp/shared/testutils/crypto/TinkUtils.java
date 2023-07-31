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

package com.google.scp.shared.testutils.crypto;

import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toJsonCiphertext;

import com.google.crypto.tink.Aead;
import com.google.crypto.tink.HybridEncrypt;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.hybrid.EciesAeadHkdfPrivateKeyManager;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.scp.operator.cpio.cryptoclient.testing.FakePrivateKeyFetchingService;
import java.io.IOException;
import java.security.GeneralSecurityException;

public class TinkUtils {

  private final Aead aead;
  private final FakePrivateKeyFetchingService keyFetchingService;

  public TinkUtils(Aead aead, FakePrivateKeyFetchingService keyFetchingService)
      throws GeneralSecurityException {
    this.keyFetchingService = keyFetchingService;
    this.aead = aead;
    HybridConfig.register();
  }

  /**
   * Creates a new encryption key, updates KeyFetchingService to return an encypted version of that
   * key, then returns the corresponding HybridEncrypt interface that encrypts using that key.
   */
  private HybridEncrypt setupEncryptionKey() throws GeneralSecurityException, IOException {
    KeysetHandle privateKeysetHandle =
        KeysetHandle.generateNew(
            EciesAeadHkdfPrivateKeyManager.rawEciesP256HkdfHmacSha256Aes128GcmCompressedTemplate());
    String ciphertext = toJsonCiphertext(privateKeysetHandle, aead);
    keyFetchingService.setResponse(ciphertext);
    return privateKeysetHandle.getPublicKeysetHandle().getPrimitive(HybridEncrypt.class);
  }

  public byte[] getCiphertext(String plaintext) throws GeneralSecurityException, IOException {
    HybridEncrypt hybridEncrypt = setupEncryptionKey();
    byte[] resp = hybridEncrypt.encrypt(plaintext.getBytes(), null);
    return resp;
  }
}
