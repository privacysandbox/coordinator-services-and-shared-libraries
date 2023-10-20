package com.google.scp.protocol.avro;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.common.truth.Truth.assertThat;

import com.google.common.collect.ImmutableList;
import org.apache.avro.Schema;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class AvroResultsSchemaSupplierTest {

  // Under test
  AvroResultsSchemaSupplier avroResultsSchemaSupplier;

  @Before
  public void setUp() {
    avroResultsSchemaSupplier = new AvroResultsSchemaSupplier();
  }

  @Test
  public void testHasCorrectSchema() {
    // no setup

    Schema schema = avroResultsSchemaSupplier.get();

    // Get the list of the field names and check that they are the expected fields
    ImmutableList<String> fieldNames =
        schema.getFields().stream().map(Schema.Field::name).collect(toImmutableList());
    assertThat(fieldNames).containsExactly("key", "value");
  }

  @Test
  public void testSchemaIsCached() {
    // no setup

    Schema firstGet = avroResultsSchemaSupplier.get();
    Schema secondGet = avroResultsSchemaSupplier.get();

    assertThat(firstGet).isSameInstanceAs(secondGet);
  }
}
