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

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.crypto.tink.integration.awskmsv2.AwsKmsV2Aead;
import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import com.google.scp.shared.crypto.tink.kmstoolenclave.KmsToolEnclaveAead;
import java.util.Optional;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import software.amazon.awssdk.http.SdkHttpClient;

@RunWith(JUnit4.class)
public final class AwsTinkUtilTest {
  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  private static final String VALID_ARN =
      "arn:aws:kms:us-west-2:123456789012:key/123abcde-f123-4567-890a-bcdef123456";
  private static final String VALID_ARN_URI = "aws-kms://" + VALID_ARN;

  @Mock private AwsSessionCredentialsProvider credentialsProvider;
  @Mock private SdkHttpClient httpClient;

  @Test
  public void parseKmsUri_correctArn() {
    var arn = AwsTinkUtil.parseKmsUri(VALID_ARN_URI);

    assertThat(arn.region().get()).isEqualTo("us-west-2");
    assertThat(arn.accountId().get()).isEqualTo("123456789012");
    assertThat(arn.resource().resourceType().get()).isEqualTo("key");
  }

  @Test
  public void parseKmsUri_throwsIfInvalidPrefix() {
    // No prefix.
    assertThrows(IllegalArgumentException.class, () -> AwsTinkUtil.parseKmsUri(VALID_ARN));
    // Prefix for another cloud.
    assertThrows(
        IllegalArgumentException.class, () -> AwsTinkUtil.parseKmsUri("gcp-kms://" + VALID_ARN));
  }

  @Test
  public void parseKmsUri_throwsIfInvalidArn() {
    var invalidUri = "aws-kms://invalidarn";

    assertThrows(IllegalArgumentException.class, () -> AwsTinkUtil.parseKmsUri(invalidUri));
  }

  @Test
  public void getEnclaveAeadSelector_returnsKmsToolAead() throws Exception {
    var aeadSelector = AwsTinkUtil.getEnclaveAeadSelector(credentialsProvider);

    var aead = aeadSelector.getAead(VALID_ARN_URI);

    assertThat(aead).isInstanceOf(KmsToolEnclaveAead.class);
  }

  @Test
  public void getEnclaveAeadSelector_throwsIfInvalidArn() throws Exception {
    var aeadSelector = AwsTinkUtil.getEnclaveAeadSelector(credentialsProvider);

    // Bare ARN
    assertThrows(IllegalArgumentException.class, () -> aeadSelector.getAead(VALID_ARN));
    // invalid ARN
    assertThrows(
        IllegalArgumentException.class, () -> aeadSelector.getAead("aws-kms://invalidArn"));
  }

  @Test
  public void getKmsAeadSelector_returnsAwsKmsV2Aead() throws Exception {
    var aeadSelector =
        AwsTinkUtil.getKmsAeadSelector(credentialsProvider, httpClient, Optional.empty());

    var aead = aeadSelector.getAead(VALID_ARN_URI);

    assertThat(aead).isInstanceOf(AwsKmsV2Aead.class);
  }

  @Test
  public void getKmsAeadSelector_throwsIfInvalidArn() throws Exception {
    var aeadSelector =
        AwsTinkUtil.getKmsAeadSelector(credentialsProvider, httpClient, Optional.empty());

    // Bare ARN
    assertThrows(IllegalArgumentException.class, () -> aeadSelector.getAead(VALID_ARN));
    // invalid ARN
    assertThrows(
        IllegalArgumentException.class, () -> aeadSelector.getAead("aws-kms://invalidArn"));
  }
}
