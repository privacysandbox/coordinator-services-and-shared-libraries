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

package com.google.scp.operator.worker.decryption.hybrid;

import com.google.common.io.ByteSource;
import com.google.crypto.tink.HybridDecrypt;
import java.io.IOException;
import java.security.GeneralSecurityException;

/**
 * Implementation of {@code DecryptionCipher} that uses a key provided by the aggregate KMS service.
 */
public final class HybridDecryptionCipher {

  private final HybridDecrypt hybridDecrypt;

  // Empty byte array to indicate that there's no Associated Data
  // TODO(b/199187471) find out if this will be populated
  static final byte[] CONTEXT_INFO = new byte[] {};

  public static HybridDecryptionCipher of(HybridDecrypt hybridDecrypt) {
    return new HybridDecryptionCipher(hybridDecrypt);
  }

  private HybridDecryptionCipher(HybridDecrypt hybridDecrypt) {
    this.hybridDecrypt = hybridDecrypt;
  }

  public ByteSource decrypt(ByteSource encryptedPayload) throws PayloadDecryptionException {
    try {
      return ByteSource.wrap(hybridDecrypt.decrypt(encryptedPayload.read(), CONTEXT_INFO));
    } catch (GeneralSecurityException | IOException e) {
      throw new PayloadDecryptionException(e);
    }
  }

  public static class PayloadDecryptionException extends Exception {

    public PayloadDecryptionException(Throwable cause) {
      super(cause);
    }
  }
}
