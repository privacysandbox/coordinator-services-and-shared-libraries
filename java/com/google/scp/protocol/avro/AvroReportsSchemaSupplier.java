package com.google.scp.protocol.avro;

import javax.inject.Singleton;

/** Supplier of the Avro {@code Schema} for the reports file schema */
@Singleton
public final class AvroReportsSchemaSupplier extends AvroSchemaSupplier {

  @Override
  String schemaResourcePath() {
    return "/operator/protocol/avro/reports.avsc";
  }
}
