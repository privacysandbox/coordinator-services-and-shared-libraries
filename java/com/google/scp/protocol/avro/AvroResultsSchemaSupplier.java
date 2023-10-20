package com.google.scp.protocol.avro;

import javax.inject.Singleton;

/** Supplies the Avro {@code Schema} for the results file. */
@Singleton
public final class AvroResultsSchemaSupplier extends AvroSchemaSupplier {

  @Override
  String schemaResourcePath() {
    return "/operator/protocol/avro/results.avsc";
  }
}
