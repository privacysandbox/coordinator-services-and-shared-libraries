/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.operator.worker.model.serdes.proto;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth8.assertThat;

import com.google.common.io.ByteSource;
import com.google.scp.operator.worker.model.Report;
import com.google.scp.operator.worker.model.serdes.ReportSerdes;
import com.google.scp.operator.worker.testing.FakeReportGenerator;
import java.util.Optional;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class ProtoReportSerdesTest {

  private ReportSerdes reportSerdes;
  private Report report;

  @Before
  public void setUp() {
    reportSerdes = new ProtoReportSerdes();
    report = FakeReportGenerator.generate(1);
  }

  @Test
  public void returnsSameObject() {
    ByteSource serializedReport = reportSerdes.reverse().convert(Optional.of(report));
    Optional<Report> result = reportSerdes.convert(serializedReport);

    assertThat(result).isPresent();
    assertThat(report).isEqualTo(result.get());
  }

  @Test
  public void returnsSameObjectWhenEmptyByteSource() throws Exception {
    ByteSource empty = ByteSource.empty();

    Optional<Report> converted = reportSerdes.convert(empty);
    ByteSource convertedBack = reportSerdes.reverse().convert(converted);

    assertThat(convertedBack.read()).isEqualTo(empty.read());
  }

  @Test
  public void returnsSameObjectWhenEmptyOptional() throws Exception {
    Optional<Report> empty = Optional.empty();

    ByteSource converted = reportSerdes.reverse().convert(empty);
    Optional<Report> convertedBack = reportSerdes.convert(converted);

    assertThat(convertedBack).isEmpty();
  }

  @Test
  public void returnsEmptyByteSourceForEmptyReport() throws Exception {
    Optional<Report> emptyReport = Optional.empty();

    ByteSource converted = reportSerdes.reverse().convert(emptyReport);

    assertThat(converted.read()).isEmpty();
  }

  @Test
  public void returnsEmptyReportForEmptyByteSource() {
    ByteSource emptyBytes = ByteSource.empty();

    Optional<Report> converted = reportSerdes.convert(emptyBytes);

    assertThat(converted).isEmpty();
  }

  @Test
  public void returnsEmptyReportForFailedDeserialization() {
    ByteSource garbageBytes = ByteSource.wrap(new byte[] {0x00, 0x01});

    Optional<Report> converted = reportSerdes.convert(garbageBytes);

    assertThat(converted).isEmpty();
  }
}
