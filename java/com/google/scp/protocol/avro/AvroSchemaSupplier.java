package com.google.scp.protocol.avro;

import com.google.common.base.Suppliers;
import java.io.IOException;
import java.io.InputStream;
import java.util.function.Supplier;
import org.apache.avro.Schema;

/**
 * Supplies an Avro {@code Schema}
 *
 * <p>Reads the JSON schema file that is baked into the JAR as a resource and parses it. This class
 * is abstract so that subclasses can specify which schema file will be provided.
 */
abstract class AvroSchemaSupplier implements Supplier<Schema> {

  // Memoized supplier to provide the parsed schema. The schema can't change during runtime so it
  // can be memoized. Parsing logic is only executed once.
  private final Supplier<Schema> memoizedSupplier;

  AvroSchemaSupplier() {
    memoizedSupplier = Suppliers.memoize(() -> parseAvroSchemaFromResources(schemaResourcePath()));
  }

  /** Provide the {@code Schema} for the results file. */
  @Override
  public Schema get() {
    return memoizedSupplier.get();
  }

  /**
   * The path to the JSON schema file that will be provided.
   *
   * <p>Abstract so that subclasses implement it to provide the path they need.
   */
  abstract String schemaResourcePath();

  /** Reads the JSON schema file and parses it to the {@code Schema} object. */
  private static Schema parseAvroSchemaFromResources(String resource) {
    try (InputStream inputStream = AvroSchemaSupplier.class.getResourceAsStream(resource)) {
      Schema.Parser parser = new Schema.Parser();
      return parser.parse(inputStream);
    } catch (IOException e) {
      throw new AvroSchemaParseException(e);
    }
  }

  /**
   * Exception for failed schema parsing.
   *
   * <p>This is extends {@code RuntimeException} since failed parsing should be a fatal exception.
   */
  private static final class AvroSchemaParseException extends RuntimeException {
    private AvroSchemaParseException(Throwable cause) {
      super(cause);
    }
  }
}
