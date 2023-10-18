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

package com.google.scp.operator.cpio.cryptoclient.gcp;

import com.google.auto.value.AutoValue;
import com.google.common.annotations.Beta;
import com.google.crypto.tink.Aead;
import com.google.protobuf.ByteString;
import com.google.scp.shared.util.KeysetHandleSerializerUtil;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.Base64;
import java.util.Optional;

/** Holds configuration parameters for {@code GcpKmsDecryptionKeyServiceModule}. */
@AutoValue
public abstract class GcpKmsDecryptionKeyServiceConfig {

  /** Returns an instance of the {@code GcpKmsDecryptionKeyServiceConfig.Builder} class. */
  public static Builder builder() {
    return new AutoValue_GcpKmsDecryptionKeyServiceConfig.Builder()
        .setCoordinatorBKmsKeyUri(Optional.empty())
        .setCoordinatorCloudfunctionUrl(Optional.empty());
  }

  /** Key URI of the coordinatorA gcloud KMS key. */
  public abstract String coordinatorAKmsKeyUri();

  /** Cloudfunction url for encryption key service */
  public abstract Optional<String> coordinatorCloudfunctionUrl();

  /** Key URI of the coordinatorB gcloud KMS key. */
  public abstract Optional<String> coordinatorBKmsKeyUri();

  /**
   * Encoded string for the coordinatorA KeysetHandle. This is a stopgap solution and will be
   * removed eventually once it's no longer needed.
   *
   * <p>TODO: delete once local KMS has been implemented and can replace this stopgap feature.
   */
  @Beta
  protected abstract Optional<String> coordinatorAEncodedKeysetHandle();

  /**
   * Encoded string for the coordinatorB KeysetHandle. This is a stopgap solution and will be
   * removed eventually once it's no longer needed.
   *
   * <p>TODO: delete once local KMS has been implemented and can replace this stopgap feature.
   */
  @Beta
  protected abstract Optional<String> coordinatorBEncodedKeysetHandle();

  /**
   * Aead for coordinatorA. If this is present, will be used instead of coordinatorAKmsKeyUri. This
   * is a stopgap solution and will be removed eventually once it's no longer needed.
   *
   * <p>TODO: delete once local KMS has been implemented and can replace this stopgap feature.
   */
  @Beta
  public Optional<Aead> coordinatorAAead() {
    return coordinatorAEncodedKeysetHandle()
        .map(GcpKmsDecryptionKeyServiceConfig::getAeadFromEncodedKeysetHandle);
  }

  /**
   * Aead for coordinatorb. If this is present, will be used instead of coordinatorBKmsKeyUri. This
   * is a stopgap solution and will be removed eventually once it's no longer needed.
   *
   * <p>TODO: delete once local KMS has been implemented and can replace this stopgap feature.
   */
  @Beta
  public Optional<Aead> coordinatorBAead() {
    return coordinatorBEncodedKeysetHandle()
        .map(GcpKmsDecryptionKeyServiceConfig::getAeadFromEncodedKeysetHandle);
  }

  /**
   * Uses the KeysetHandle that's encoded in the encodedKeysetHandle string to retrieve an Aead.
   * This is a stopgap solution and will be removed eventually once it's no longer needed.
   */
  private static Aead getAeadFromEncodedKeysetHandle(String encodedKeysetHandle) {
    ByteString keysetHandleByteString =
        ByteString.copyFrom(Base64.getDecoder().decode(encodedKeysetHandle));
    try {
      return KeysetHandleSerializerUtil.fromBinaryCleartext(keysetHandleByteString)
          .getPrimitive(Aead.class);
    } catch (GeneralSecurityException | IOException e) {
      throw new RuntimeException(e);
    }
  }

  /** Builder class for the {@code GcpKmsDecryptionKeyServiceConfig} class. */
  @AutoValue.Builder
  public abstract static class Builder {

    /** Set the key URI of coordinatorA gcloud KMS key. */
    public abstract Builder setCoordinatorAKmsKeyUri(String coordinatorAKmsKeyUri);

    /** Set the cloudfunction url for coordinator encryption key service */
    public abstract Builder setCoordinatorCloudfunctionUrl(
        Optional<String> coordinatorCloudfunctionUrl);

    /** Set the key URI of coordinatorB gcloud KMS key. */
    public abstract Builder setCoordinatorBKmsKeyUri(Optional<String> coordinatorBKmsKeyUri);

    /**
     * Encoded string for the coordinatorA KeysetHandle. If this is present, will be used instead of
     * coordinatorAKmsKeyUri. This is a stopgap solution and will be removed eventually once it's no
     * longer needed.
     *
     * <p>TODO: delete once local KMS has been implemented and can replace this stopgap feature.
     */
    @Beta
    public abstract Builder setCoordinatorAEncodedKeysetHandle(
        Optional<String> encodedKeysetHandle);

    /**
     * Encoded string for the coordinatorB KeysetHandle. If this is present, will be used instead of
     * coordinatorBKmsKeyUri. This is a stopgap solution and will be removed eventually once it's no
     * longer needed.
     *
     * <p>TODO: delete once local KMS has been implemented and can replace this stopgap feature.
     */
    @Beta
    public abstract Builder setCoordinatorBEncodedKeysetHandle(
        Optional<String> encodedKeysetHandle);

    /**
     * Uses the builder to construct a new instance of the {@code GcpKmsDecryptionKeyServiceConfig}
     * class.
     */
    public abstract GcpKmsDecryptionKeyServiceConfig build();
  }
}
