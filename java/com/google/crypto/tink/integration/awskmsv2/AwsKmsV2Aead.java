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

package com.google.crypto.tink.integration.awskmsv2;

import com.google.common.base.Splitter;
import com.google.common.collect.ImmutableMap;
import com.google.crypto.tink.Aead;
import java.security.GeneralSecurityException;
import java.util.List;
import software.amazon.awssdk.core.SdkBytes;
import software.amazon.awssdk.services.kms.KmsClient;
import software.amazon.awssdk.services.kms.model.DecryptRequest;
import software.amazon.awssdk.services.kms.model.DecryptResponse;
import software.amazon.awssdk.services.kms.model.EncryptRequest;
import software.amazon.awssdk.services.kms.model.EncryptRequest.Builder;
import software.amazon.awssdk.utils.BinaryUtils;

/**
 * Aead implementation based on V2 of the AWS Java Api.
 *
 * <p>Based heavily on
 * https://github.com/google/tink/blob/master/java_src/src/main/java/com/google/crypto/tink/integration/awskms/AwsKmsAead.java
 *
 * <p>TODO(b/188813845): Upstream this to Tink.
 */
public final class AwsKmsV2Aead implements Aead {
  private final KmsClient kmsClient;
  private final String keyArn;

  public AwsKmsV2Aead(KmsClient kmsClient, String keyArn) {
    this.kmsClient = kmsClient;
    this.keyArn = keyArn;
  }

  @Override
  public byte[] encrypt(final byte[] plaintext, final byte[] associatedData) {
    Builder req =
        EncryptRequest.builder().keyId(keyArn).plaintext(SdkBytes.fromByteArray(plaintext));
    if (associatedData != null && associatedData.length != 0) {
      ImmutableMap<String, String> context =
          ImmutableMap.of("associatedData", BinaryUtils.toHex(associatedData));
      req = req.encryptionContext(context);
    }
    return kmsClient.encrypt(req.build()).ciphertextBlob().asByteArray();
  }

  @Override
  public byte[] decrypt(final byte[] ciphertext, final byte[] associatedData)
      throws GeneralSecurityException {
    DecryptRequest.Builder req =
        DecryptRequest.builder().keyId(keyArn).ciphertextBlob(SdkBytes.fromByteArray(ciphertext));
    if (associatedData != null && associatedData.length != 0) {
      ImmutableMap<String, String> context =
          ImmutableMap.of("associatedData", BinaryUtils.toHex(associatedData));
      req = req.encryptionContext(context);
    }
    DecryptResponse result = kmsClient.decrypt(req.build());
    // In AwsKmsAead.decrypt() it is important to check the returned KeyId against the one
    // previously configured. If we don't do this, the possibility exists for the ciphertext to
    // be replaced by one under a key we don't control/expect, but do have decrypt permissions
    // on.
    // The check is disabled if keyARN is not in key ARN format.
    // See https://docs.aws.amazon.com/kms/latest/developerguide/concepts.html#key-id.
    if (isKeyArnFormat(keyArn) && !result.keyId().equals(keyArn)) {
      throw new GeneralSecurityException("Decryption failed: wrong key id");
    }

    return result.plaintext().asByteArray();
  }

  /** Returns {@code true} if {@code keyArn} is in key ARN format. */
  private static boolean isKeyArnFormat(String keyArn) {
    // Example: arn:aws:kms:us-west-2:123456789012:key/12345678-90ab-cdef-1234-567890abcdef
    List<String> tokens = Splitter.on(':').splitToList(keyArn);
    return tokens.size() == 6 && tokens.get(5).startsWith("key/");
  }
}
