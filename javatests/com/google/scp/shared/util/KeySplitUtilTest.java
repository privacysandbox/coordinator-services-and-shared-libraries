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
import static org.junit.Assert.assertThrows;

import com.google.common.collect.ImmutableList;
import com.google.crypto.tink.HybridDecrypt;
import com.google.crypto.tink.HybridEncrypt;
import com.google.crypto.tink.KeyTemplate;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.protobuf.ByteString;
import java.security.GeneralSecurityException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class KeySplitUtilTest {

  private static final KeyTemplate keyTemplate;

  static {
    try {
      HybridConfig.register();
      keyTemplate = KeyParams.getDefaultKeyTemplate();
    } catch (GeneralSecurityException e) {
      throw new RuntimeException("Error initializing tink.", e);
    }
  }

  @Test
  public void xorSplitBytes_nPieces() throws Exception {
    var message = ByteString.copyFromUtf8("Foo");

    var oneSplit = KeySplitUtil.xorSplitBytes(message, 1);
    var twoSplits = KeySplitUtil.xorSplitBytes(message, 2);
    var threeSplits = KeySplitUtil.xorSplitBytes(message, 3);

    assertThat(oneSplit.size()).isEqualTo(1);
    assertThat(twoSplits.size()).isEqualTo(2);
    assertThat(threeSplits.size()).isEqualTo(3);
  }

  @Test
  public void xorSplitBytes_uniquePieces() throws Exception {
    var message = ByteString.copyFromUtf8("verylongsecretfornocollisions");

    var splits = KeySplitUtil.xorSplitBytes(message, 3);

    // Verify the original message isn't included in the response.
    splits.forEach(split -> assertThat(split).isNotEqualTo(message));
    // Verify all pieces are unique.
    assertThat(splits.stream().distinct().count()).isEqualTo(3);
  }

  @Test
  public void reconstructXorBytes_success() throws Exception {
    // 111 100
    byte[] a = {7, 4};
    // 100 010
    byte[] b = {4, 2};
    // 011 110
    byte[] expected = {3, 6};

    var inputStream = ImmutableList.of(ByteString.copyFrom(a), ByteString.copyFrom(b));

    assertThat(KeySplitUtil.reconstructXorBytes(inputStream))
        .isEqualTo(ByteString.copyFrom(expected));
  }

  @Test
  public void reconstructXorBytes_mismatchThrowsIllegalArgument() throws Exception {
    var growingList = ImmutableList.of(ByteString.copyFromUtf8("1"), ByteString.copyFromUtf8("22"));
    var shrinkingList =
        ImmutableList.of(ByteString.copyFromUtf8("22"), ByteString.copyFromUtf8("1"));

    assertThrows(
        IllegalArgumentException.class, () -> KeySplitUtil.reconstructXorBytes(growingList));
    assertThrows(
        IllegalArgumentException.class, () -> KeySplitUtil.reconstructXorBytes(shrinkingList));
  }

  @Test
  public void xorSplitBytes_reconstructXorBytes() throws Exception {
    var message = ByteString.copyFromUtf8("Test message");

    // Test that a reconstruction succeeds with a 1-split, 2-split, and 3-split.
    for (var size : ImmutableList.of(1, 2, 3)) {
      var splits = KeySplitUtil.xorSplitBytes(message, size);
      assertThat(KeySplitUtil.reconstructXorBytes(splits)).isEqualTo(message);
    }
  }

  /**
   * Verifies that a KeysetHandle reconstructed using reconstructXorKeysetHandle is able to decrypt
   * a payload using the original KeysetHandle.
   */
  @Test
  public void xorSplitKeysetHandle_reconstructXorKeysetHandle() throws Exception {
    var originalKeysetHandle = KeysetHandle.generateNew(keyTemplate);
    var message = "Foo".getBytes();
    var encryptedPayload =
        originalKeysetHandle
            .getPublicKeysetHandle()
            .getPrimitive(HybridEncrypt.class)
            .encrypt(message, null);

    var splits = KeySplitUtil.xorSplit(originalKeysetHandle, 3);
    var reconstructedKeysetHandle = KeySplitUtil.reconstructXorKeysetHandle(splits);
    var decryptedPayload =
        reconstructedKeysetHandle.getPrimitive(HybridDecrypt.class).decrypt(encryptedPayload, null);

    assertThat(decryptedPayload).isEqualTo(message);
  }
}
