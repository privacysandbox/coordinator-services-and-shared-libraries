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

package com.google.scp.shared.crypto.tink.aws;

import com.google.crypto.tink.integration.awskmsv2.AwsKmsV2Aead;
import com.google.crypto.tink.subtle.Validators;
import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.crypto.tink.kmstoolenclave.KmsToolEnclaveAead;
import java.net.URI;
import java.util.Optional;
import software.amazon.awssdk.arns.Arn;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.http.SdkHttpClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.kms.KmsClient;

/**
 * AWS-specific util functions for operating on Tink-specific resource names.
 *
 * <p>Common converters between tink-specific "aws-kms://arn:..." URIs that point to AWS resources
 * and the corresponding Tink primitives that operate on those AWS resources.
 */
public final class AwsTinkUtil {

  private AwsTinkUtil() {}

  /**
   * Returns an AeadSelector that uses the normal Java SDK. If attestation is needed, use {@code
   * getEnclaveAeadSelector} instead.
   *
   * <p>The returned AeadSelector will throw an IllegalArgumentException is passed an ARN not in the
   * format of "aws-kms://arn:..." or does not have a valid region associated with it.
   */
  public static CloudAeadSelector getKmsAeadSelector(
      AwsCredentialsProvider credentialProvider,
      SdkHttpClient httpClient,
      Optional<URI> kmsEndpointOverride) {
    return (kmsKeyArn) -> {
      var arn = parseKmsUri(kmsKeyArn);

      var kmsClient =
          KmsClient.builder()
              .credentialsProvider(credentialProvider)
              .region(getRegion(arn))
              .httpClient(httpClient)
              .applyMutation((client) -> kmsEndpointOverride.ifPresent(client::endpointOverride))
              .build();
      return new AwsKmsV2Aead(kmsClient, arn.toString());
    };
  }

  /**
   * Returns an AeadSelector that uses the KMS Enclave CLI.
   *
   * <p><b>Only works inside a Nitro enclave</b>, use {@code getKmsAeadSelector} if not running
   * inside of an enclave.
   *
   * <p>The returned AeadSelector will throw an IllegalArgumentException is passed an ARN not in the
   * format of "aws-kms://arn:..." or does not have a valid region associated with it.
   */
  public static CloudAeadSelector getEnclaveAeadSelector(
      AwsSessionCredentialsProvider credentialsProvider) {
    return (kmsKeyArn) -> {
      var arn = parseKmsUri(kmsKeyArn);

      return new KmsToolEnclaveAead(credentialsProvider, getRegion(arn));
    };
  }

  /**
   * Parses a string in the Tink format of a URI with a scheme of "aws-kms" (e.g.
   * "aws-kms://arn:aws:kms:us-east-1:111122223333:key/1234...") and returns the corresponding ARN.
   *
   * <p>AWS doesn't accept ARNs in the format of "aws-kms://...", this is a Tink-specific construct.
   *
   * @throws IllegalArgumentException if {@code kmsKeyUri} is invalid.
   */
  public static Arn parseKmsUri(String kmsUri) {
    var arn = Validators.validateKmsKeyUriAndRemovePrefix("aws-kms://", kmsUri);
    return Arn.fromString(arn);
  }

  /**
   * Extracts the region from the ARN or throw an {@code IllegalArgumentException} if there isn't
   * one present.
   */
  private static Region getRegion(Arn arn) {
    if (arn.region().isEmpty()) {
      throw new IllegalArgumentException(
          String.format("Provided ARN (%s) doesn't contain a region.", arn.toString()));
    }
    return Region.of(arn.region().get());
  }
}
