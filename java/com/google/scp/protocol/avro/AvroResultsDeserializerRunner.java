package com.google.scp.protocol.avro;

import com.google.common.collect.ImmutableList;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Inject;
import com.google.scp.operator.worker.model.Fact;
import com.google.scp.operator.worker.testing.AvroResultsFileReader;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.file.Paths;

final class AvroResultsDeserializerRunner {
  AvroResultsFileReader reader;

  @Inject
  public AvroResultsDeserializerRunner(AvroResultsFileReader reader) {
    this.reader = reader;
  }

  public static void main(String[] args) throws IOException {
    AvroResultsDeserializerRunner runner =
        Guice.createInjector(new AbstractModule() {})
            .getInstance(AvroResultsDeserializerRunner.class);

    runner.runResultDeserialization(args[0], args[1]);
  }

  private void runResultDeserialization(String fileInputPath, String fileOutputPath)
      throws IOException {
    ImmutableList<Fact> writtenResults = reader.readAvroResultsFile(Paths.get(fileInputPath));
    BufferedWriter writer = new BufferedWriter(new FileWriter(fileOutputPath));
    writer.write("Agg Facts report for: " + fileOutputPath + "\n");
    for (Fact fact : writtenResults) {
      writer.write(formatOutput(fact));
    }
    writer.close();
  }

  private static String formatOutput(Fact fact) {
    return fact.key() + " " + fact.value();
  }
}
