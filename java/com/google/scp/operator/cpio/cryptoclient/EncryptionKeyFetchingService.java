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

import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;

/** Interface responsible for fetching encrypted key splits from the Key Handling Service. */
public interface EncryptionKeyFetchingService {

  /** Returns the encryption key for the given key ID. */
  EncryptionKey fetchEncryptionKey(String keyId) throws EncryptionKeyFetchingServiceException;

  /** Represents an exception thrown by the {@code EncryptionKeyFetchingService} class. */
  final class EncryptionKeyFetchingServiceException extends Exception {
    /** Creates a new instance from a {@code Throwable}. */
    public EncryptionKeyFetchingServiceException(Throwable cause) {
      super(cause);
    }

    /** Creates a new instance from a message String. */
    public EncryptionKeyFetchingServiceException(String message) {
      super(message);
    }

    /** Creates a new instance from a message String and a Throwable. */
    public EncryptionKeyFetchingServiceException(String message, Throwable cause) {
      super(message, cause);
    }
  }
}
