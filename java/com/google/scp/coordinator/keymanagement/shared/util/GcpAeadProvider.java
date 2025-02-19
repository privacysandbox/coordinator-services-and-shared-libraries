/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.keymanagement.shared.util;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.common.annotations.Beta;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.integration.awskmsv2.AwsKmsV2Client;
import com.google.crypto.tink.integration.gcpkms.GcpKmsClient;
import com.google.protobuf.ByteString;
import com.google.scp.shared.aws.credsprovider.FederatedAwsCredentialsProvider;
import com.google.scp.shared.util.KeysetHandleSerializerUtil;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.Base64;
import java.util.Optional;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;

/** Defines helper methods for constructing GCP Aead implementations. */
public final class GcpAeadProvider {

  /** Get GCP Aead from URI with an option to use credentials. */
  public static Aead getGcpAead(String kmsKeyUri, Optional<GoogleCredentials> credentials) {
    GcpKmsClient client = new GcpKmsClient();
    try {
      if (credentials.isPresent()) {
        client.withCredentials(credentials.get());
      } else {
        client.withDefaultCredentials();
      }
      return client.getAead(kmsKeyUri);
    } catch (GeneralSecurityException e) {
      throw new RuntimeException(
          String.format("Error getting gcloud Aead with uri %s.", kmsKeyUri), e);
    }
  }

  /** Get AWS Aead from URI using default GCP credentials via federation. */
  public static Aead getAwsAead(String kmsKeyUri, String kmsRoleArn) {
    AwsCredentialsProvider provider = new FederatedAwsCredentialsProvider(kmsRoleArn);
    try {
      return new AwsKmsV2Client().withCredentialsProvider(provider).getAead(kmsKeyUri);
    } catch (GeneralSecurityException e) {
      throw new RuntimeException(
          String.format("Error getting AWS Aead with uri %s.", kmsKeyUri), e);
    }
  }

  /** Get Aead from the encoded KeysetHandle string. */
  @Beta
  public static Aead getAeadFromEncodedKeysetHandle(String encodedKeysetHandle) {
    ByteString keysetHandleByteString =
        ByteString.copyFrom(Base64.getDecoder().decode(encodedKeysetHandle));
    try {
      return KeysetHandleSerializerUtil.fromBinaryCleartext(keysetHandleByteString)
          .getPrimitive(Aead.class);
    } catch (GeneralSecurityException | IOException e) {
      throw new RuntimeException(e);
    }
  }
}
