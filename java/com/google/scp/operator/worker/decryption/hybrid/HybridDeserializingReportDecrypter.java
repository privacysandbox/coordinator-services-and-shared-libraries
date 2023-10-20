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

import com.google.common.io.ByteSource;
import com.google.inject.Inject;
import com.google.scp.operator.worker.decryption.RecordDecrypter;
import com.google.scp.operator.worker.decryption.hybrid.HybridDecryptionCipher.PayloadDecryptionException;
import com.google.scp.operator.worker.decryption.hybrid.HybridDecryptionCipherFactory.CipherCreationException;
import com.google.scp.operator.worker.model.EncryptedReport;
import com.google.scp.operator.worker.model.Report;
import com.google.scp.operator.worker.model.serdes.ReportSerdes;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Decrypts and deserializes {@code EncryptedReports} using {@code HybridDecryptionCipher} and
 * {@code ReportSerdes}
 */
public final class HybridDeserializingReportDecrypter implements RecordDecrypter {

  private final HybridDecryptionCipherFactory decryptionCipherFactory;
  private final ReportSerdes reportSerdes;
  private final Logger logger = LoggerFactory.getLogger(HybridDeserializingReportDecrypter.class);

  @Inject
  public HybridDeserializingReportDecrypter(
      HybridDecryptionCipherFactory decryptionCipherFactory, ReportSerdes reportSerdes) {
    this.decryptionCipherFactory = decryptionCipherFactory;
    this.reportSerdes = reportSerdes;
  }

  @Override
  public Report decryptSingleReport(EncryptedReport encryptedReport) throws DecryptionException {
    try {
      HybridDecryptionCipher decryptionCipher =
          decryptionCipherFactory.decryptionCipherFor(encryptedReport);
      ByteSource decryptedPayload = decryptionCipher.decrypt(encryptedReport.payload());
      return reportSerdes
          .convert(decryptedPayload)
          .orElseThrow(
              () ->
                  new PayloadDecryptionException(
                      new IllegalArgumentException("Decrypted payload could not be deserialized")));
    } catch (PayloadDecryptionException | CipherCreationException e) {
      throw new DecryptionException(e);
    }
  }
}
