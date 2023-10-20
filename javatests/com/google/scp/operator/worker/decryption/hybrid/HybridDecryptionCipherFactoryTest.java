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

package com.google.scp.operator.worker.decryption.hybrid;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.acai.TestScoped;
import com.google.common.io.ByteSource;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService.KeyFetchException;
import com.google.scp.operator.cpio.cryptoclient.testing.FakeDecryptionKeyService;
import com.google.scp.operator.worker.decryption.hybrid.HybridDecryptionCipherFactory.CipherCreationException;
import com.google.scp.operator.worker.model.EncryptedReport;
import java.util.UUID;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class HybridDecryptionCipherFactoryTest {

  @Rule public final Acai acai = new Acai(TestEnv.class);

  // Under test
  @Inject HybridDecryptionCipherFactory hybridDecryptionCipherFactory;

  @Inject FakeDecryptionKeyService fakeDecryptionKeyService;

  @Test
  public void testThrowsIfReportDecryptionKeyIdMissing() {
    EncryptedReport encryptedReport =
        EncryptedReport.builder().setPayload(ByteSource.empty()).build();

    assertThrows(
        IllegalArgumentException.class,
        () -> hybridDecryptionCipherFactory.decryptionCipherFor(encryptedReport));
  }

  @Test
  public void testThrowsOnFailedFetch() {
    EncryptedReport encryptedReport =
        EncryptedReport.builder()
            .setPayload(ByteSource.empty())
            .setDecryptionKeyId(UUID.randomUUID().toString())
            .build();
    fakeDecryptionKeyService.setShouldThrow(true);

    CipherCreationException exception =
        assertThrows(
            CipherCreationException.class,
            () -> hybridDecryptionCipherFactory.decryptionCipherFor(encryptedReport));
    assertThat(exception).hasCauseThat().isInstanceOf(KeyFetchException.class);
  }

  /** Test that DecryptionKeyService is requested with the right key */
  @Test
  public void testRequestsWithTheRightKey() throws CipherCreationException {
    String decryptionKeyId = UUID.randomUUID().toString();
    // Keys passed to decryptionKeyService are strings, not UUIDs.
    String expectedKeyId = decryptionKeyId;
    EncryptedReport encryptedReport =
        EncryptedReport.builder()
            .setPayload(ByteSource.empty())
            .setDecryptionKeyId(decryptionKeyId)
            .build();

    HybridDecryptionCipher decryptionCipher =
        hybridDecryptionCipherFactory.decryptionCipherFor(encryptedReport);

    assertThat(decryptionCipher).isInstanceOf(HybridDecryptionCipher.class);
    assertThat(fakeDecryptionKeyService.getLastDecryptionKeyIdUsed()).isEqualTo(expectedKeyId);
  }

  private static final class TestEnv extends AbstractModule {

    @Override
    protected void configure() {
      bind(FakeDecryptionKeyService.class).in(TestScoped.class);
      bind(DecryptionKeyService.class).to(FakeDecryptionKeyService.class);
    }
  }
}
