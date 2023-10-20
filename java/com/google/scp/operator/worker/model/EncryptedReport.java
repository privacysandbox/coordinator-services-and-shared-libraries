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

package com.google.scp.operator.worker.model;

import com.google.auto.value.AutoValue;
import com.google.common.io.ByteSource;
import java.util.Optional;

/** Representation of a single encrypted report */
@AutoValue
public abstract class EncryptedReport {

  public static Builder builder() {
    return new AutoValue_EncryptedReport.Builder();
  }

  /** The encrypted bytes of the report */
  public abstract ByteSource payload();

  /** The optional key used for hybrid decryption */
  // TODO(b/196647643) Change this field from UUID to string.
  public abstract Optional<String> decryptionKeyId();

  @AutoValue.Builder
  public abstract static class Builder {

    public abstract Builder setPayload(ByteSource payload);

    public abstract Builder setDecryptionKeyId(String decryptionKeyId);

    public abstract EncryptedReport build();
  }
}
