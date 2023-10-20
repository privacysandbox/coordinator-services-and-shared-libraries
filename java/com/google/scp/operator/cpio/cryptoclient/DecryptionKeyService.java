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

package com.google.scp.operator.cpio.cryptoclient;

import com.google.crypto.tink.HybridDecrypt;
import com.google.scp.operator.cpio.cryptoclient.model.ErrorReason;

/** Interface responsible for retrieving and decrypting keys from the KMS */
public interface DecryptionKeyService {

  /**
   * Retrieve a key from the KMS using the key identifier.
   *
   * <p>Returns the HybridDecrypt interface as opposed to a KeysetHandle because getKey is in the
   * hot path of the decryption logic and there is measureable overhead transforming a KeysetHandle
   * into its HybridDecrypt counterpart.
   */
  HybridDecrypt getDecrypter(String decryptionKeyId) throws KeyFetchException;

  /** Represents an exception thrown by the {@code DecryptionKeyService} class. */
  final class KeyFetchException extends Exception {
    public final ErrorReason reason;

    /** Creates a new instance of the {@code KeyFetchException} class. */
    public KeyFetchException(Throwable cause, ErrorReason reason) {
      super(cause);
      this.reason = reason;
    }

    /** Creates a new instance of the {@code KeyFetchException} class. */
    public KeyFetchException(String message, ErrorReason reason, Throwable cause) {
      super(message, cause);
      this.reason = reason;
    }

    /** Creates a new instance from a message String. */
    public KeyFetchException(String message, ErrorReason reason) {
      super(message);
      this.reason = reason;
    }

    /** Returns {@code ErrorReason} for the exception. */
    public ErrorReason getReason() {
      return reason;
    }

    /** Returns a String representation of the class. */
    @Override
    public String toString() {
      return String.format("ERROR: %s\n%s", reason, super.toString());
    }
  }
}
