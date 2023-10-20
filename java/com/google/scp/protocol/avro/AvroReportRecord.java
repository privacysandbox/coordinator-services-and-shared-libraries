package com.google.scp.protocol.avro;

import com.google.auto.value.AutoValue;
import com.google.common.io.ByteSource;

@AutoValue
public abstract class AvroReportRecord {

  public static AvroReportRecord create(ByteSource encryptedShare, String decryptionKeyId) {
    return new AutoValue_AvroReportRecord(encryptedShare, decryptionKeyId);
  }

  public abstract ByteSource encryptedShare();

  public abstract String decryptionKeyId();
}
