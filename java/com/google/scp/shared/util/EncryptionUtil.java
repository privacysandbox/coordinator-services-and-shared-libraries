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

import com.google.crypto.tink.Aead;
import com.google.crypto.tink.KeyTemplates;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.aead.AeadConfig;
import com.google.protobuf.ByteString;
import java.io.IOException;
import java.security.GeneralSecurityException;

/** Contains encryption related functionality */
public class EncryptionUtil {

  /** Needed to initialize Tink Aead encryption/decryption */
  static {
    try {
      AeadConfig.register();
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Error initializing tink.");
    }
  }

  /**
   * Returns a new serialized Aead key encrypted with the provided Aead
   *
   * @return
   */
  public ByteString generateSymmetricEncryptedKey(Aead aead)
      throws GeneralSecurityException, IOException {
    KeysetHandle keysetHandle = KeysetHandle.generateNew(KeyTemplates.get("AES128_GCM"));
    return KeysetHandleSerializerUtil.toBinaryCiphertext(keysetHandle, aead);
  }
}
