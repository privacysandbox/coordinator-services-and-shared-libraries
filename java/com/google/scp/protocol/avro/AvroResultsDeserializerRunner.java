/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
