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
import com.google.crypto.tink.BinaryKeysetReader;
import com.google.crypto.tink.BinaryKeysetWriter;
import com.google.crypto.tink.CleartextKeysetHandle;
import com.google.crypto.tink.JsonKeysetReader;
import com.google.crypto.tink.JsonKeysetWriter;
import com.google.crypto.tink.KeysetHandle;
import com.google.protobuf.ByteString;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.security.GeneralSecurityException;

/** Common Serialization and Deserization functions for operating on a Tink {@link KeysetHandle}. */
public final class KeysetHandleSerializerUtil {

  private KeysetHandleSerializerUtil() {}

  /**
   * Returns the binary-serialized and encrypted version of the provided KeysetHandle.
   *
   * @see #fromBinaryCiphertext(ByteString, Aead) to do the inverse operation.
   */
  public static ByteString toBinaryCiphertext(KeysetHandle keysetHandle, Aead aead)
      throws IOException, GeneralSecurityException {
    var keyStream = new ByteArrayOutputStream();
    keysetHandle.write(BinaryKeysetWriter.withOutputStream(keyStream), aead);
    return ByteString.copyFrom(keyStream.toByteArray());
  }

  /** Reverses an operation performed by {@link #toBinaryCiphertext(KeysetHandle, Aead)}. */
  public static KeysetHandle fromBinaryCiphertext(ByteString inputBytes, Aead aead)
      throws GeneralSecurityException, IOException {
    return KeysetHandle.read(BinaryKeysetReader.withBytes(inputBytes.toByteArray()), aead);
  }

  /**
   * Returns the binary-serialized version of the provided KeysetHandle.
   *
   * @see #fromBinaryCleartext(KeysetHandle) to do the inverse operation.
   */
  public static ByteString toBinaryCleartext(KeysetHandle keysetHandle) throws IOException {
    var keyStream = new ByteArrayOutputStream();
    CleartextKeysetHandle.write(keysetHandle, BinaryKeysetWriter.withOutputStream(keyStream));
    return ByteString.copyFrom(keyStream.toByteArray());
  }

  /** Reverses an operation performed by {@link #toBinaryCleartext(KeysetHandle) */
  public static KeysetHandle fromBinaryCleartext(ByteString inputBytes)
      throws GeneralSecurityException, IOException {
    return CleartextKeysetHandle.read(BinaryKeysetReader.withBytes(inputBytes.toByteArray()));
  }

  /**
   * Returns the JSON encoded version of the provided KeysetHandle as a string, used for encrypting
   * and serializing a KeysetHandle in a way that is human readable.
   *
   * @see #fromJsonCiphertext(String, Aead) To do the inverse operation.
   * @deprecated Use {@link #toBinaryCipherText(KeysetHandle, Aead)} for new use cases. Most of the
   *     JSON payload is encrypted so there is not much value in representing an encrypted
   *     KeysetHandle in JSON for human readability purposes.
   */
  @Deprecated
  public static String toJsonCiphertext(KeysetHandle keysetHandle, Aead aead)
      throws GeneralSecurityException, IOException {
    ByteArrayOutputStream keyStream = new ByteArrayOutputStream();
    keysetHandle.write(JsonKeysetWriter.withOutputStream(keyStream), aead);
    return keyStream.toString();
  }

  /** Reverses an operation performed by {@link #toJsonCiphertext(KeysetHandle,Aead)}. */
  public static KeysetHandle fromJsonCiphertext(String input, Aead aead)
      throws GeneralSecurityException, IOException {
    return KeysetHandle.read(JsonKeysetReader.withString(input), aead);
  }

  /**
   * Returns the JSON encoded version of the provided KeysetHandle as a string, used for serializing
   * a KeysetHandle in a way that is human-readable.
   *
   * @see #fromJsonCleartext(String) To do the inverse operation.
   */
  public static String toJsonCleartext(KeysetHandle keysetHandle) throws IOException {
    ByteArrayOutputStream keyStream = new ByteArrayOutputStream();
    CleartextKeysetHandle.write(keysetHandle, JsonKeysetWriter.withOutputStream(keyStream));
    return keyStream.toString();
  }

  /** Reverses an operation performed by {@link #toJsonCleartext(KeysetHandle)} */
  public static KeysetHandle fromJsonCleartext(String input)
      throws GeneralSecurityException, IOException {
    return CleartextKeysetHandle.read(JsonKeysetReader.withString(input));
  }
}
