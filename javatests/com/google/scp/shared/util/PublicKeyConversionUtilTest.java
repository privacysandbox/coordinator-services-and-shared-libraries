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

import com.google.crypto.tink.CleartextKeysetHandle;
import com.google.crypto.tink.HybridDecrypt;
import com.google.crypto.tink.HybridEncrypt;
import com.google.crypto.tink.JsonKeysetReader;
import com.google.crypto.tink.KeyTemplates;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.hybrid.HybridConfig;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class PublicKeyConversionUtilTest {

  @BeforeClass
  public static void init() throws Exception {
    HybridConfig.register();
  }

  /**
   * JSON-serialized public keysethandle.
   *
   * <pre>
   *   tinkey create-public-keyset --out-format json \
   *      --in <(tinkey create-keyset \
   *        --key-template DHKEM_X25519_HKDF_SHA256_HKDF_SHA256_CHACHA20_POLY1305_RAW \
   *        --out-format json)
   * </pre>
   */
  private static final String PUBLIC_KEY_EXAMPLE =
      "{\"primaryKeyId\":1135265500,\"key\":[{\"keyData\":{\"typeUrl\":"
          + "\"type.googleapis.com/google.crypto.tink.HpkePublicKey\",\"value\":\""
          + "EgYIARABGAMaIIFHueDZa+urua0sfOqHw8dOncI6iPBa2mFiG3yH7+Nk"
          + "\",\"keyMaterialType\":"
          + "\"ASYMMETRIC_PUBLIC\"},\"status\":\"ENABLED\",\"keyId\":1135265500,"
          + "\"outputPrefixType\":\"RAW\"}]}";

  /**
   * Base64-encoded version of the underlying key inside PUBLIC_KEY_EXAMPLE
   *
   * <pre>
   *   var input = <keyData.value field copied from PUBLIC_KEY_EXAMPLE>
   *   var key = HpkePublicKey.parseFrom(Base64.getDecoder().decode(input));
   *   System.out.println(new String(Base64.getEncoder().encode(key.getPublicKey().toByteArray())));
   * </pre>
   */
  private static final String RAW_PUBLIC_KEY = "gUe54Nlr66u5rSx86ofDx06dwjqI8FraYWIbfIfv42Q=";

  @Test
  public void getPublicKey_success() throws Exception {
    var keysetHandle = CleartextKeysetHandle.read(JsonKeysetReader.withString(PUBLIC_KEY_EXAMPLE));

    var publicKey = PublicKeyConversionUtil.getPublicKey(keysetHandle);

    assertThat(publicKey).isEqualTo(RAW_PUBLIC_KEY);
  }

  @Test
  public void getPublicKey_throwsIfPrivateKey() throws Exception {
    var privateKeysetHandle = KeysetHandle.generateNew(KeyParams.getDefaultKeyTemplate());

    assertThrows(
        IllegalArgumentException.class,
        () -> PublicKeyConversionUtil.getPublicKey(privateKeysetHandle));
  }

  @Test
  public void getPublicKey_throwsIfInvalidPublicKey() throws Exception {
    // Key with invalid hpke parameters.
    var incompatibleHpkeKey =
        KeysetHandle.generateNew(
                KeyTemplates.get("DHKEM_X25519_HKDF_SHA256_HKDF_SHA256_AES_256_GCM_RAW"))
            .getPublicKeysetHandle();
    // Different type of hybrid key.
    var eciesKey =
        KeysetHandle.generateNew(KeyTemplates.get("ECIES_P256_HKDF_HMAC_SHA256_AES128_GCM_RAW"))
            .getPublicKeysetHandle();

    assertThrows(
        IllegalStateException.class,
        () -> PublicKeyConversionUtil.getPublicKey(incompatibleHpkeKey));
    assertThrows(IllegalStateException.class, () -> PublicKeyConversionUtil.getPublicKey(eciesKey));
  }

  @Test
  public void getKeysetHandle_success() throws Exception {
    KeysetHandle keysetHandle = PublicKeyConversionUtil.getKeysetHandle(RAW_PUBLIC_KEY);

    assertThat(PublicKeyConversionUtil.getPublicKey(keysetHandle)).isEqualTo(RAW_PUBLIC_KEY);
  }

  /**
   * Tests that a payload encrypted with a public key converted to and back from a raw public key is
   * able to be decrypted by the original key.
   */
  @Test
  public void reconversionTest() throws Exception {
    var keysetHandle = KeysetHandle.generateNew(KeyParams.getDefaultKeyTemplate());

    var convertedPublicKeysetHandle =
        PublicKeyConversionUtil.getKeysetHandle(
            PublicKeyConversionUtil.getPublicKey(keysetHandle.getPublicKeysetHandle()));
    var hybridEncrypt = convertedPublicKeysetHandle.getPrimitive(HybridEncrypt.class);
    var hybridDecrypt = keysetHandle.getPrimitive(HybridDecrypt.class);

    var testData = "foo".getBytes();
    var ciphertext = hybridEncrypt.encrypt(testData, null);
    var decryptedText = hybridDecrypt.decrypt(ciphertext, null);

    assertThat(decryptedText).isEqualTo("foo".getBytes());
  }
}
