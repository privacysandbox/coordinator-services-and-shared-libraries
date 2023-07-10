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

package com.google.scp.operator.cpio.cryptoclient.aws;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.crypto.tink.HybridDecrypt;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService.KeyFetchException;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyServiceImpl;
import com.google.scp.operator.cpio.cryptoclient.testing.FakePrivateKeyFetchingService;
import com.google.scp.shared.testutils.aws.AwsHermeticTestHelper;
import com.google.scp.shared.testutils.crypto.AwsTinkUtilsModule;
import com.google.scp.shared.testutils.crypto.TinkUtils;
import java.util.UUID;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.testcontainers.containers.localstack.LocalStackContainer;
import org.testcontainers.containers.wait.strategy.Wait;

/** Tests using simulated responses from PrivateKeyVendingService and localstack to simulate KMS. */
@RunWith(JUnit4.class)
public class AwsKmsDecryptionKeyServiceTest {

  @ClassRule
  public static LocalStackContainer localstack =
      AwsHermeticTestHelper.createContainer(LocalStackContainer.Service.KMS);

  @Rule public final Acai acai = new Acai(TestEnv.class);

  @Inject TinkUtils tinkTestUtils;
  @Inject DecryptionKeyServiceImpl awsKmsDecryptionKeyService;
  @Inject FakePrivateKeyFetchingService keyFetchingService;

  @Before
  public void setUp() throws Exception {
    localstack.waitingFor(Wait.forHealthcheck());
  }

  @Test
  public void testDecryption() throws Exception {
    String plaintext = "{psuedo conversion event}";
    byte[] ciphertext = tinkTestUtils.getCiphertext(plaintext);

    HybridDecrypt hybridDecrypt =
        awsKmsDecryptionKeyService.getDecrypter(UUID.randomUUID().toString());
    byte[] decryptedText = hybridDecrypt.decrypt(ciphertext, null);

    assertThat(decryptedText).isEqualTo(plaintext.getBytes());
  }

  @Test
  public void testInvalidDecryption() throws Exception {
    // Invalid key, unable to be decrypted.
    keyFetchingService.setResponse("{}");

    assertThrows(
        KeyFetchException.class,
        () -> awsKmsDecryptionKeyService.getDecrypter(UUID.randomUUID().toString()));
  }

  private static final class TestEnv extends AbstractModule {

    @Override
    protected void configure() {
      install(AwsTinkUtilsModule.withLocalstack(localstack));
    }
  }
}
