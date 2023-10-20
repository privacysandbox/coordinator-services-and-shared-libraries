package com.google.scp.protocol.avro;

import java.io.IOException;
import java.io.InputStream;
import javax.inject.Inject;
import org.apache.avro.file.DataFileStream;
import org.apache.avro.generic.GenericDatumReader;

/** Produces {@code AvroReportsReader}s for given input streams */
public final class AvroReportsReaderFactory {

  private final AvroReportsSchemaSupplier schemaSupplier;

  @Inject
  public AvroReportsReaderFactory(AvroReportsSchemaSupplier schemaSupplier) {
    this.schemaSupplier = schemaSupplier;
  }

  public AvroReportsReader create(InputStream in) throws IOException {
    return new AvroReportsReader(
        new DataFileStream<>(in, new GenericDatumReader<>(schemaSupplier.get())));
  }
}
