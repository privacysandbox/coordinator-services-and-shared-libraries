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

import static com.google.scp.shared.util.KeysetHandleSerializerUtil.fromBinaryCleartext;
import static com.google.scp.shared.util.KeysetHandleSerializerUtil.toBinaryCleartext;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.ImmutableList;
import com.google.crypto.tink.KeysetHandle;
import com.google.protobuf.ByteString;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.security.SecureRandom;

/** Utility methods for creating and reconstructing key splits. */
public final class KeySplitUtil {
  private static final SecureRandom random = new SecureRandom();

  private KeySplitUtil() {}

  /**
   * Serializes the provided keysetHandle and generates a split using {@link
   * #xorSplitBytes(ByteString,int)}.
   *
   * <p>The inverse operation can be performed using {@link
   * #reconstructXorKeysetHandle(ImmutableList<ByteString>)}
   *
   * <p>The returned list will contain ByteStrings of equal size.
   */
  public static ImmutableList<ByteString> xorSplit(KeysetHandle keysetHandle, int numPieces)
      throws IOException {
    var message = toBinaryCleartext(keysetHandle);

    return xorSplitBytes(message, numPieces);
  }

  /**
   * Returns the original KeysetHandle split using {@link #xorSplit(KeysetHandle,int)}
   *
   * @throws IllegalArgumentException if the lengths of the passed in ByteStrings do not match.
   */
  public static KeysetHandle reconstructXorKeysetHandle(ImmutableList<ByteString> pieces)
      throws GeneralSecurityException, IOException {
    var reconstructedBytes = reconstructXorBytes(pieces);

    return fromBinaryCleartext(reconstructedBytes);
  }

  /**
   * Splits the provided message into N pieces by XORing the message with (numPieces - 1) pieces of
   * random data. Returns each of the pieces of random data as well as the result of XORing the
   * provided message with each of the pieces of random data.
   *
   * <p>The inverse operation can be performed using {@link
   * #reconstructXorBytes(ImmutableList<ByteString>)}
   *
   * <p>The returned list will return ByteStrings of equal size.
   */
  @VisibleForTesting
  static ImmutableList<ByteString> xorSplitBytes(ByteString message, int numPieces) {
    var splitsBuilder = ImmutableList.<ByteString>builder();
    var xorResult = message.toByteArray();

    // Create N - 1 pieces because the XOR'd result is the Nth piece.
    for (var i = 0; i < numPieces - 1; i++) {
      byte[] randomBytes = new byte[message.size()];
      random.nextBytes(randomBytes);

      xorResult = xor(xorResult, randomBytes);
      splitsBuilder.add(ByteString.copyFrom(randomBytes));
    }

    splitsBuilder.add(ByteString.copyFrom(xorResult));

    return splitsBuilder.build();
  }

  /**
   * Returns the original message split using {@link #xorSplitBytes(ByteString,int)}.
   *
   * @throws IllegalArgumentException if the lengths of the passed in ByteStrings do not match.
   */
  @VisibleForTesting
  static ByteString reconstructXorBytes(ImmutableList<ByteString> pieces) {
    var xorSecret = pieces.get(0).toByteArray();

    // Skip i = 0 because xorSecret already has the first member.
    for (var i = 1; i < pieces.size(); i++) {
      var nextPiece = pieces.get(i).toByteArray();
      if (xorSecret.length != nextPiece.length) {
        throw new IllegalArgumentException("Input ByteString list has mismatched sizes.");
      }
      xorSecret = xor(xorSecret, nextPiece);
    }

    return ByteString.copyFrom(xorSecret);
  }

  /** Util method for XORing two byte arrays. Assumes that a and b have the same length. */
  private static byte[] xor(byte[] a, byte[] b) {
    var response = new byte[a.length];
    for (var i = 0; i < a.length; i++) {
      response[i] = (byte) (a[i] ^ b[i]);
    }
    return response;
  }
}
