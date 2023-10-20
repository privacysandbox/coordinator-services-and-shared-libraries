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

import static com.google.common.base.Preconditions.checkArgument;

import com.google.inject.Inject;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService.KeyFetchException;
import com.google.scp.operator.worker.model.EncryptedReport;

/**
 * Provides {@code HybridDecryptionCipher}s for the hybrid decryption scheme.
 *
 * <p>Inspects the provided {@code EncryptedReport} for the key used to decrypt, retrieves the key
 * it needs from the {@code DecryptionKeyService}, and constructs a decryption cipher using that
 * key.
 */
public final class HybridDecryptionCipherFactory {

  private final DecryptionKeyService decryptionKeyService;

  @Inject
  public HybridDecryptionCipherFactory(DecryptionKeyService decryptionKeyService) {
    this.decryptionKeyService = decryptionKeyService;
  }

  /** Retrieves the key needed to decrypt the report and constucts a decryption cipher for it. */
  public HybridDecryptionCipher decryptionCipherFor(EncryptedReport encryptedReport)
      throws CipherCreationException {
    checkArgument(
        encryptedReport.decryptionKeyId().isPresent(),
        "encryptedReport must have a decryptionKeyId present to create DecryptionCipher");

    try {
      var decryptionKey =
          decryptionKeyService.getDecrypter(encryptedReport.decryptionKeyId().get());
      return HybridDecryptionCipher.of(decryptionKey);
    } catch (KeyFetchException e) {
      throw new CipherCreationException(e);
    }
  }

  public static final class CipherCreationException extends Exception {

    public CipherCreationException(Throwable cause) {
      super(cause);
    }
  }
}
