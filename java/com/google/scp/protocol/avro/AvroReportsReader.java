package com.google.scp.protocol.avro;

import com.google.common.io.ByteSource;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Optional;
import java.util.stream.Stream;
import org.apache.avro.AvroTypeException;
import org.apache.avro.file.DataFileStream;
import org.apache.avro.generic.GenericRecord;

/**
 * Reader that provides reports from an Avro file following the defined schema.
 *
 * <p>The schema is provided by the schema supplier. For convenience, the reader object can be
 * created through the factory, which allows the supplier to he bound ith dependency injection, thus
 * requiring only the input stream to be passed.
 */
public final class AvroReportsReader implements AutoCloseable {

  private final DataFileStream<GenericRecord> streamReader;

  AvroReportsReader(DataFileStream<GenericRecord> streamReader) {
    this.streamReader = streamReader;
  }

  public Stream<AvroReportRecord> streamRecords() throws InvalidAvroSchemaException {
    Optional<AvroReportRecord> firstRecord;

    try {
      // Read the first record separately, to test if the schema is correct
      firstRecord = readRecordForStreaming();
    } catch (AvroTypeException e) {
      throw new InvalidAvroSchemaException("Error reading AVRO record due to schema mismatch.", e);
    }

    Stream<Optional<AvroReportRecord>> remainingRecords =
        Stream.generate(this::readRecordForStreaming);

    return Stream.concat(Stream.of(firstRecord), remainingRecords)
        .takeWhile(Optional::isPresent)
        .map(Optional::get);
  }

  /** Reads metadata string specified by the key (returns empty optional if not available) */
  public Optional<String> getMeta(String key) {
    return Optional.ofNullable(streamReader.getMetaString(key));
  }

  private Optional<AvroReportRecord> readRecordForStreaming() {
    if (streamReader.hasNext()) {
      return Optional.of(reportRecordFromGeneric(streamReader.next()));
    }

    return Optional.empty();
  }

  @Override
  public void close() throws IOException {
    streamReader.close();
  }

  private static AvroReportRecord reportRecordFromGeneric(GenericRecord record) {
    return AvroReportRecord.create(
        ByteSource.wrap(((ByteBuffer) record.get("encryptedShare")).array()),
        ((CharSequence) record.get("decryptionKeyId")).toString());
  }

  public static final class InvalidAvroSchemaException extends Exception {
    private InvalidAvroSchemaException(String message, Throwable cause) {
      super(message, cause);
    }
  }
}
