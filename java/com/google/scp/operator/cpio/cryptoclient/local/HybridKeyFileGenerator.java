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

package com.google.scp.operator.cpio.cryptoclient.local;

import com.google.crypto.tink.BinaryKeysetReader;
import com.google.crypto.tink.BinaryKeysetWriter;
import com.google.crypto.tink.CleartextKeysetHandle;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.hybrid.EciesAeadHkdfPrivateKeyManager;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.GeneralSecurityException;

/**
 * Generates a file with the proto containing the Hybrid key pair (or does nothing if the file
 * already exists).
 */
public final class HybridKeyFileGenerator {

  private HybridKeyFileGenerator() {}

  /**
   * Generates a hybrid keyset handle at the given path, unless a key already exists there.
   *
   * <p>Note that this KeysetHandle has both the public and private key.
   *
   * @return whether a new key was generated; false is there was something already existing.
   */
  public static boolean generateKeysetHandle(Path path)
      throws IOException, GeneralSecurityException {
    // TODO: possibly enforce this path to exist if only aggregation is being run. Otherwise, the
    // payload cannot be decrypted.
    // A key of some size already exists there, skip generating.
    if (Files.exists(path) && hasKey(path)) {
      return false;
    }
    // Generate new key and write to file.
    KeysetHandle keysetHandle =
        KeysetHandle.generateNew(
            EciesAeadHkdfPrivateKeyManager.rawEciesP256HkdfHmacSha256Aes128GcmCompressedTemplate());
    CleartextKeysetHandle.write(
        keysetHandle, BinaryKeysetWriter.withOutputStream(Files.newOutputStream(path)));
    return true;
  }

  /** Reads {@link KeysetHandle} from proto stored in binary-wire format at {@param path}. */
  public static KeysetHandle readKeysetHandle(Path path)
      throws IOException, GeneralSecurityException {
    return CleartextKeysetHandle.read(
        BinaryKeysetReader.withInputStream(Files.newInputStream(path)));
  }

  /**
   * Returns true if there is a Keyset at {@param path}.
   *
   * <p>If the key could not be processed for whatever reason, returns false.
   */
  private static boolean hasKey(Path path) {
    try {
      readKeysetHandle(path);
    } catch (IOException | GeneralSecurityException e) {
      // Unable to process the key.
      return false;
    }
    return true;
  }
}
