package com.google.scp.protocol.avro;

import static com.google.common.truth.Truth.assertThat;

import org.apache.avro.Schema;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class AvroReportsSchemaSupplierTest {

  AvroReportsSchemaSupplier schemaSupplier;

  @Before
  public void setUp() {
    schemaSupplier = new AvroReportsSchemaSupplier();
  }

  @Test
  public void correctSchema() {
    Schema schema = schemaSupplier.get();

    assertThat(schema.getFields()).hasSize(2);
    Schema.Field encryptedShareField = schema.getFields().get(0);
    Schema.Field decryptionKeyIdField = schema.getFields().get(1);
    assertThat(encryptedShareField.name()).isEqualTo("encryptedShare");
    assertThat(decryptionKeyIdField.name()).isEqualTo("decryptionKeyId");
  }

  @Test
  public void schemaIsCached() {
    Schema schema = schemaSupplier.get();
    Schema schemaAgain = schemaSupplier.get();

    assertThat(schema).isSameInstanceAs(schemaAgain);
  }
}
