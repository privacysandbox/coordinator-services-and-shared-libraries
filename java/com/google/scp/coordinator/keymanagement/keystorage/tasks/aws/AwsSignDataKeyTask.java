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

package com.google.scp.coordinator.keymanagement.keystorage.tasks.aws;

import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeyPublicKeySign;
import static com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.DataKeyPublicKeyVerify;

import com.google.common.base.Joiner;
import com.google.crypto.tink.PublicKeySign;
import com.google.crypto.tink.PublicKeyVerify;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.SignDataKeyTask;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.time.Clock;
import java.time.Duration;
import java.time.Instant;
import java.time.format.DateTimeParseException;
import java.util.Base64;

/**
 * Creates and validates signatures for {@link DataKey}s
 *
 * <p>Note: this implementation uses signatures rather than Message Authentication Codes in order to
 * be testable with LocalStack which does not yet have HMAC support.
 */
public final class AwsSignDataKeyTask implements SignDataKeyTask {
  // Keys are valid if they were created this far in the past or future.
  private static final Duration KEY_VALIDITY_WINDOW = Duration.ofHours(1);
  private final PublicKeySign publicKeySign;
  private final PublicKeyVerify publicKeyVerify;
  private final Clock clock;

  @Inject
  public AwsSignDataKeyTask(
      Clock clock,
      @DataKeyPublicKeySign PublicKeySign publicKeySign,
      @DataKeyPublicKeyVerify PublicKeyVerify publicKeyVerify) {
    this.publicKeySign = publicKeySign;
    this.publicKeyVerify = publicKeyVerify;
    this.clock = clock;
  }

  public DataKey signDataKey(DataKey dataKey) throws ServiceException, IllegalArgumentException {
    if (!dataKey.getDataKeyContext().isEmpty()
        || !dataKey.getMessageAuthenticationCode().isEmpty()) {
      throw new IllegalArgumentException(
          "Attempting to sign key which already contains signature.");
    }
    if (dataKey.getEncryptedDataKey().isEmpty() || dataKey.getEncryptedDataKeyKekUri().isEmpty()) {
      throw new IllegalArgumentException(
          "Attempting to sign key which is missing a kek URI or encrypted payload");
    }
    String context = Instant.now(clock).toString();
    byte[] message = toSignableMessage(dataKey, context).getBytes(StandardCharsets.UTF_8);
    try {
      String signature = new String(Base64.getEncoder().encode(publicKeySign.sign(message)));

      return dataKey.toBuilder()
          .setDataKeyContext(context)
          .setMessageAuthenticationCode(signature)
          .build();
    } catch (GeneralSecurityException e) {
      throw new ServiceException(Code.INTERNAL, "SIGNATURE_EXCEPTION", "Signature failed", e);
    }
  }

  public void verifyDataKey(DataKey dataKey) throws ServiceException {
    if (dataKey.getMessageAuthenticationCode().isEmpty() || dataKey.getDataKeyContext().isEmpty()) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          "SIGNATURE_EXCEPTION",
          "Provided DataKey missing signature or context.");
    }
    validateContext(dataKey);

    byte[] message =
        toSignableMessage(dataKey, dataKey.getDataKeyContext()).getBytes(StandardCharsets.UTF_8);
    byte[] signature = Base64.getDecoder().decode(dataKey.getMessageAuthenticationCode());
    try {
      publicKeyVerify.verify(signature, message);
    } catch (GeneralSecurityException e) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT, "SIGNATURE_EXCEPTION", "Signature validation failed", e);
    }
  }

  private static String toSignableMessage(DataKey dataKey, String context) {
    // context%%encryptedKey%%uri
    return Joiner.on("%%")
        .join(context, dataKey.getEncryptedDataKey(), dataKey.getEncryptedDataKeyKekUri());
  }

  /** Throws if the provided data key is expired. */
  private void validateContext(DataKey dataKey) throws ServiceException {
    var now = Instant.now(clock);

    Instant keyInstant;
    try {
      keyInstant = Instant.parse(dataKey.getDataKeyContext());
    } catch (DateTimeParseException e) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT,
          "DATA_KEY_CONTEXT_EXCEPTION",
          "The provided data key could not be parsed.",
          e);
    }
    var timeDifference = Duration.between(now, keyInstant).abs();

    if (timeDifference.compareTo(KEY_VALIDITY_WINDOW) > 0) {
      throw new ServiceException(
          Code.INVALID_ARGUMENT, "DATA_KEY_CONTEXT_EXCEPTION", "The provided data key is expired.");
    }
  }
}
