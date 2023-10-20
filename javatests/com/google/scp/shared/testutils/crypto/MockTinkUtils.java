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

import static com.google.scp.shared.util.KeysetHandleSerializerUtil.fromBinaryCleartext;

import com.google.crypto.tink.HybridEncrypt;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.protobuf.ByteString;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.Base64;

public class MockTinkUtils {

  private final String encryptedKeyBase64;
  private final String decryptedKeyBase64;
  private final String aeadKeySetJson;

  /**
   * Mocked Tink utils for hybrid encryption/decryption
   *
   * <p>Use mocked {@code} object in your test: @Mock private Aead aead; ...
   * when(aead.decrypt(any(byte[].class), any(byte[].class)))
   * .thenReturn(mockTinkUtils.getDecryptedKey()); when(aead.encrypt(any(byte[].class),
   * any(byte[].class))) .thenReturn(mockTinkUtils.getEncryptedKey());
   */
  public MockTinkUtils() throws GeneralSecurityException {
    encryptedKeyBase64 =
        "AQICAHiuStyq7W9/DQFL6rofWENTET/4FcRss+J9ka6bEiuJbwGlhyQvC"
            + "HR+tas04Eu/ek9VAAABdDCCAXAGCSqGSIb3DQEHBqCCAWEwggFdAgEAMIIBVgYJKoZIhvcN"
            + "AQcBMB4GCWCGSAFlAwQBLjARBAzOevmv3IPyM5qI52cCARCAggEnF+/iO4vQUsOuREe84MQ"
            + "3fNZ7R4tKqQnia+3MHQJh4ivB6HsTgofrJdeISaAf+Hd4wO3sNTcm/mtHPOBNFhAzX6O7G6"
            + "teGLU1iQW1fOyVZnhG8UpRrWXANqQfoFyLU8NONlKyIkpCO1ZykvRWx5jyA3T/c1+0GF5R6"
            + "23xd7ioY+ZjM13WXJZpVQBVIGsdUAF0ziMdL+VaoUtS9dNioTMAwsCgcZLhSL/+IJNmAviJ"
            + "a+YRE66aTVwREWHWMLpTpMMD8C43TWbKkuwjhDJFzCaF18p2GO2bl+CHYs0Cws+IID1q/IL"
            + "v87DG/pnPNcxwq9LaMlG7okpD2kNFfAqSpTYlP9n/0nOXzgXA2l5w2nYqP7Tn6+YLCCtgw+"
            + "9Oq00kibUT0Q8tbbnwDw==";
    decryptedKeyBase64 =
        "CKW6wMcEEoMCCvYBCj50eXBlLmdvb2dsZWFwaXMuY29tL2dvb2dsZS5jc"
            + "nlwdG8udGluay5FY2llc0FlYWRIa2RmUHJpdmF0ZUtleRKxARKLARJECgQIAhADEjoSOAo"
            + "wdHlwZS5nb29nbGVhcGlzLmNvbS9nb29nbGUuY3J5cHRvLnRpbmsuQWVzR2NtS2V5EgIQE"
            + "BgBGAEaIQCobHI4xov3osW1bfWCbiZApo4J8OkWFNmlRcgWidtRLyIgJYc2c3jgmfGJqO"
            + "fjJqPW+bMm4R6yRICfQHfA1mumy7UaIQDxXKDLphlHtczAlIxwg+ccIu02VCYVTag4t6UP"
            + "NC5fixgCEAEYpbrAxwQgAQ==";

    aeadKeySetJson =
        "{  \"keysetInfo\": {      \"primaryKeyId\": 1223695653,      \"keyInfo\": [{         "
            + " \"typeUrl\": \"type.googleapis.com/google.crypto.tink.EciesAeadHkdfPrivateKey\",   "
            + "       \"outputPrefixType\": \"TINK\",          \"keyId\": 1223695653,         "
            + " \"status\": \"ENABLED\"      }]  },  \"encryptedKeyset\":"
            + " \""
            + encryptedKeyBase64
            + "\""
            + "}";

    HybridConfig.register();
  }

  // returns encrypted private key
  public byte[] getEncryptedKey() {
    return Base64.getDecoder().decode(encryptedKeyBase64);
  }

  // returns decrypted private key
  public byte[] getDecryptedKey() {
    return Base64.getDecoder().decode(decryptedKeyBase64);
  }

  // returns json serialized keyset
  public String getAeadKeySetJson() {
    return aeadKeySetJson;
  }

  // Encrypts the plaintext with the keyset and returns ciphered text
  public byte[] getCiphertext(String plaintext) throws GeneralSecurityException, IOException {
    KeysetHandle keySetHandle = fromBinaryCleartext(ByteString.copyFrom(getDecryptedKey()));
    HybridEncrypt hybridEncrypt =
        keySetHandle.getPublicKeysetHandle().getPrimitive(HybridEncrypt.class);
    byte[] cipheredText = hybridEncrypt.encrypt(plaintext.getBytes(), new byte[] {});
    return cipheredText;
  }
}
