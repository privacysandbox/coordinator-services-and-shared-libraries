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

import com.google.crypto.tink.KeyTemplate;
import com.google.crypto.tink.KeyTemplates;
import com.google.crypto.tink.proto.HpkeAead;
import com.google.crypto.tink.proto.HpkeKdf;
import com.google.crypto.tink.proto.HpkeKem;
import com.google.crypto.tink.proto.HpkeParams;
import com.google.crypto.tink.proto.HpkePublicKey;
import java.security.GeneralSecurityException;

/** Contains the HPKE encryption key parameters used throughout the system. */
public final class KeyParams {

  private KeyParams() {}

  /**
   * Returns the set of parameters that should be assumed for keys of the initial (1.0) release.
   *
   * <p>The HPKE parameters used in encryption/decryption are not contained in the raw keys sent to
   * clients by public key vending service and so these parameters must be synced with the
   * parameters used by clients which encrypt data.
   *
   * <p>Chrome implementation for aggregation reports can be found at
   * https://source.chromium.org/chromium/chromium/src/+/main:content/browser/aggregation_service/aggregatable_report.cc;l=227-228;drc=a7cffd0302b213659cf4c1b10f7aeb7e3954bb0e
   *
   * <p>Refer to A.2. in RFC9180 for more details.
   */
  public static HpkeParams getHpkeParams() {
    return HpkeParams.newBuilder()
        .setKem(HpkeKem.DHKEM_X25519_HKDF_SHA256)
        .setKdf(HpkeKdf.HKDF_SHA256)
        .setAead(HpkeAead.CHACHA20_POLY1305)
        .build();
  }

  /**
   * Throws if the parameters of the provide public key does not match the expected parameters for
   * keys of the initial (1.0) release.
   *
   * <p>When converting from an HpkePublicKey to its internal raw public key, the key parameter
   * information is lost and this information must be provided to recreate the HpkePublicKey. Before
   * this conversion happens, this function should be called to ensure that the parameters are
   * consistent with the parameters that would be provided when the reverse conversion occurs.
   *
   * <p>If the parameters of the original HpkePublicKey do not match the parameters of the
   * reconstructed key, the two keys will be incompatible.
   */
  public static void validateHpkeParams(HpkePublicKey publicKey) throws GeneralSecurityException {
    var params = publicKey.getParams();
    var expectedParams = getHpkeParams();
    if (!params.equals(expectedParams)) {
      throw new GeneralSecurityException(
          String.format("Unexpected params found. Expected: %s, got: %s", expectedParams, params));
    }
  }

  /**
   * Returns the key template that should be used for key generation in the initial (1.0) release.
   *
   * <p>Must be called after invoking {@link com.google.crypto.tink.hybrid.HybridConfig#register()}
   * at some point in the application lifetime.
   */
  public static KeyTemplate getDefaultKeyTemplate() throws GeneralSecurityException {
    // Note that the parameters in the below string should match those in getHpkeParams above.
    return KeyTemplates.get("DHKEM_X25519_HKDF_SHA256_HKDF_SHA256_CHACHA20_POLY1305_RAW");
  }
}
