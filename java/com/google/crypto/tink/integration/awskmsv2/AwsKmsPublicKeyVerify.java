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

package com.google.crypto.tink.integration.awskmsv2;

import com.google.crypto.tink.PublicKeyVerify;
import java.security.GeneralSecurityException;
import software.amazon.awssdk.core.SdkBytes;
import software.amazon.awssdk.services.kms.KmsClient;
import software.amazon.awssdk.services.kms.model.KmsInvalidSignatureException;
import software.amazon.awssdk.services.kms.model.VerifyRequest;

/** PublicKeyVerify implementation based AWS KMS through KmsClient. */
public final class AwsKmsPublicKeyVerify implements PublicKeyVerify {

  private final KmsClient kmsClient;
  private final String signatureAlgorithm;
  private final String signatureKeyId;

  public AwsKmsPublicKeyVerify(
      KmsClient kmsClient, String signatureKeyId, String signatureAlgorithm) {
    this.kmsClient = kmsClient;
    this.signatureKeyId = signatureKeyId;
    this.signatureAlgorithm = signatureAlgorithm;
  }

  @Override
  public void verify(final byte[] signature, byte[] data) throws GeneralSecurityException {
    var verifyRequest =
        VerifyRequest.builder()
            .message(SdkBytes.fromByteArray(data))
            .signature(SdkBytes.fromByteArray(signature))
            .signingAlgorithm(signatureAlgorithm)
            .keyId(signatureKeyId)
            .build();
    try {
      var response = kmsClient.verify(verifyRequest);
      if (!response.signatureValid()) {
        // verify() is supposed to to throw if the signature is invalid, but default LocalStack KMS
        // provider appears to incorrectly return false. Throw runtime exception if this invalid
        // behavior is observed.
        throw new IllegalStateException(
            "Received false signatureValid response from verify request");
      }
    } catch (KmsInvalidSignatureException e) {
      throw new GeneralSecurityException("Signature validation failed", e);
    }
  }
}
