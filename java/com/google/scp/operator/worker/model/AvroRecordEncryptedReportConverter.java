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

package com.google.scp.operator.worker.model;

import static com.google.common.base.Preconditions.checkArgument;

import com.google.common.base.Converter;
import com.google.scp.protocol.avro.AvroReportRecord;

/** Converts avro records to encrypted report. */
public final class AvroRecordEncryptedReportConverter
    extends Converter<AvroReportRecord, EncryptedReport> {

  @Override
  protected EncryptedReport doForward(AvroReportRecord avroRecord) {
    return EncryptedReport.builder()
        .setPayload(avroRecord.encryptedShare())
        .setDecryptionKeyId(avroRecord.decryptionKeyId())
        .build();
  }

  @Override
  protected AvroReportRecord doBackward(EncryptedReport encryptedReport) {
    checkArgument(
        encryptedReport.decryptionKeyId().isPresent(),
        "encryptedReport must have a decryptionKeyId present to create AvroReportRecord");
    return AvroReportRecord.create(
        encryptedReport.payload(), encryptedReport.decryptionKeyId().get());
  }
}
