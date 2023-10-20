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
import com.google.inject.Provides;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService;
import com.google.scp.operator.cpio.cryptoclient.testing.FakeDecryptionKeyService;
import com.google.scp.operator.worker.decryption.RecordDecrypter.DecryptionException;
import com.google.scp.operator.worker.model.EncryptedReport;
import com.google.scp.operator.worker.model.Report;
import com.google.scp.operator.worker.model.serdes.ReportSerdes;
import com.google.scp.operator.worker.model.serdes.proto.ProtoReportSerdes;
import com.google.scp.operator.worker.testing.FakeReportGenerator;
import com.google.scp.simulation.encryption.EncryptionCipher;
import com.google.scp.simulation.encryption.hybrid.HybridEncryptionCipher;
import java.security.GeneralSecurityException;
import java.util.Optional;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class HybridDeserializingReportDecrypterTest {

  @Rule public final Acai acai = new Acai(TestEnv.class);

  // Under test
  @Inject private HybridDeserializingReportDecrypter hybridDeserializingReportDecrypter;

  @Inject EncryptionCipher encryptionCipher;

  // Report used in testing
  private Report report;

  // Should decrypt and deserialize without exceptions
  private EncryptedReport encryptedReport;

  // Should decrypt correctly but fail to deserialize
  private EncryptedReport garbageReportEncryptedWithCorrectKey;

  private static final String DECRYPTION_KEY_ID = "8dab7951-e459-4a19-bd6c-d81c0a600f86";

  @Before
  public void setUp() throws Exception {
    ReportSerdes reportSerdes = new ProtoReportSerdes();
    report = FakeReportGenerator.generate(1);

    ByteSource serializedReport = reportSerdes.reverse().convert(Optional.of(report));

    encryptedReport =
        EncryptedReport.builder()
            .setPayload(encryptionCipher.encryptReport(serializedReport))
            .setDecryptionKeyId(UUID.fromString(DECRYPTION_KEY_ID).toString())
            .build();

    ByteSource garbageBytesEncryptedWithCorrectKey =
        encryptionCipher.encryptReport(ByteSource.wrap(new byte[] {0x00, 0x01}));
    garbageReportEncryptedWithCorrectKey =
        EncryptedReport.builder()
            .setPayload(garbageBytesEncryptedWithCorrectKey)
            .setDecryptionKeyId(UUID.fromString(DECRYPTION_KEY_ID).toString())
            .build();
  }

  /** Test for decrypting a single report with no errors */
  @Test
  public void testSimpleDecryption() throws Exception {
    // No setup needed

    Report decryptedReport =
        hybridDeserializingReportDecrypter.decryptSingleReport(encryptedReport);

    assertThat(decryptedReport).isEqualTo(report);
  }

  /** Test error handling for errors arising from deserialization */
  @Test
  public void testExceptionInDeserialization() {
    // No setup needed

    assertThrows(
        DecryptionException.class,
        () ->
            hybridDeserializingReportDecrypter.decryptSingleReport(
                garbageReportEncryptedWithCorrectKey));
  }

  private static final class TestEnv extends AbstractModule {

    @Override
    protected void configure() {
      bind(ReportSerdes.class).to(ProtoReportSerdes.class);
      bind(FakeDecryptionKeyService.class).in(TestScoped.class);
      bind(DecryptionKeyService.class).to(FakeDecryptionKeyService.class);
    }

    @Provides
    EncryptionCipher provideEncryptionCipher(FakeDecryptionKeyService decryptionService)
        throws GeneralSecurityException {
      return HybridEncryptionCipher.of(
          decryptionService.getKeysetHandle(DECRYPTION_KEY_ID).getPublicKeysetHandle());
    }
  }
}
