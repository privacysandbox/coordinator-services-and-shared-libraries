package com.google.scp.protocol.avro;

import java.io.OutputStream;
import javax.inject.Inject;
import org.apache.avro.file.DataFileWriter;
import org.apache.avro.generic.GenericDatumWriter;
import org.apache.avro.generic.GenericRecord;

/** Produces {@code AvroReportWriter}s */
public final class AvroReportWriterFactory {

  private final AvroReportsSchemaSupplier schemaSupplier;

  @Inject
  public AvroReportWriterFactory(AvroReportsSchemaSupplier schemaSupplier) {
    this.schemaSupplier = schemaSupplier;
  }

  public AvroReportWriter create(OutputStream outStream) {
    DataFileWriter<GenericRecord> avroWriter =
        new DataFileWriter<>(new GenericDatumWriter<>(schemaSupplier.get()));
    return new AvroReportWriter(avroWriter, outStream, schemaSupplier);
  }
}
