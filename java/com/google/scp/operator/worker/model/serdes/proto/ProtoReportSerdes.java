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

import static com.google.common.collect.ImmutableList.toImmutableList;

import com.google.common.base.Converter;
import com.google.common.io.ByteSource;
import com.google.scp.operator.worker.model.Fact;
import com.google.scp.operator.worker.model.Report;
import com.google.scp.operator.worker.model.serdes.ReportSerdes;
import com.google.scp.privacy.budgeting.model.PrivacyBudgetKey;
import com.google.scp.simulation.ReportProto;
import java.io.IOException;
import java.time.Instant;
import java.util.Optional;

/**
 * Serializes/Deserializes Reports to/from ByteSource by using an intermediate proto.
 *
 * <p>If conversion is successful then the {@code Optional} will have the result inside it. If
 * conversion fails then {@code Optional.empty()} will be returned.
 *
 * <p>Optionals are used in lieu of checked exceptions.
 */
public final class ProtoReportSerdes extends ReportSerdes {

  private static final Converter<Optional<ReportProto.Report>, Optional<Report>>
      PROTO_TO_REPORT_CONVERTER = new ProtoToReportConverter();
  private static final Converter<ByteSource, Optional<ReportProto.Report>>
      BYTE_SOURCE_TO_PROTO_CONVERTER = new BytesToProtoConverter();

  @Override
  protected Optional<Report> doForward(ByteSource byteSource) {
    return BYTE_SOURCE_TO_PROTO_CONVERTER.andThen(PROTO_TO_REPORT_CONVERTER).convert(byteSource);
  }

  @Override
  protected ByteSource doBackward(Optional<Report> report) {
    return PROTO_TO_REPORT_CONVERTER
        .reverse()
        .andThen(BYTE_SOURCE_TO_PROTO_CONVERTER.reverse())
        .convert(report);
  }

  public static final class BytesToProtoConverter
      extends Converter<ByteSource, Optional<ReportProto.Report>> {

    /**
     * Parses protobuf from @{code ByteSource}. Return {@code Optional.empty()} if the {@code
     * ByteSource} is empty.
     */
    @Override
    protected Optional<ReportProto.Report> doForward(ByteSource byteSource) {
      try {
        if (byteSource.isEmpty()) {
          // Parsing a protobuf from an empty {@code ByteSource} gives a protobuf with default
          // values set which breaks conversion reversibility
          return Optional.empty();
        }
        return Optional.of(ReportProto.Report.parseFrom(byteSource.read()));
      } catch (IOException e) {
        // Swallow the exception and return an empty {@code Optional} to indicate that the
        // deserialization failed
        // TODO: Log this exception once logging is added.
        return Optional.empty();
      }
    }

    @Override
    protected ByteSource doBackward(Optional<ReportProto.Report> report) {
      if (report.isPresent()) {
        return ByteSource.wrap(report.get().toByteArray());
      }

      return ByteSource.empty();
    }
  }

  public static final class ProtoToReportConverter
      extends Converter<Optional<ReportProto.Report>, Optional<Report>> {

    @Override
    protected Optional<Report> doForward(Optional<ReportProto.Report> reportProto) {
      return reportProto.map(ProtoToReportConverter::toReport);
    }

    @Override
    protected Optional<ReportProto.Report> doBackward(Optional<Report> report) {
      return report.map(ProtoToReportConverter::fromReport);
    }

    private static Report toReport(ReportProto.Report proto) {
      PrivacyBudgetKey privacyBudgetKey =
          PrivacyBudgetKey.builder()
              .setKey(proto.getPrivacyBudgetKey().getKey())
              .setOriginalReportTime(
                  Instant.ofEpochMilli(proto.getPrivacyBudgetKey().getOriginalReportTime()))
              .build();

      return Report.builder()
          .addAllFact(
              proto.getRecordsList().stream()
                  .map(ProtoToReportConverter::toFact)
                  .collect(toImmutableList()))
          .setAttributionDestination(proto.getAttributionDestination())
          .setAttributionReportTo(proto.getAttributionReportTo())
          .setPrivacyBudgetKey(privacyBudgetKey)
          .setOriginalReportTime(Instant.ofEpochMilli(proto.getOriginalReportTime()))
          .build();
    }

    private static Fact toFact(ReportProto.Report.Fact fact) {
      return Fact.builder().setKey(fact.getKey()).setValue(fact.getValue()).build();
    }

    private static ReportProto.Report fromReport(Report report) {
      ReportProto.PrivacyBudgetKey privacyBudgetKey =
          ReportProto.PrivacyBudgetKey.newBuilder()
              .setKey(report.privacyBudgetKey().key())
              .setOriginalReportTime(report.privacyBudgetKey().originalReportTime().toEpochMilli())
              .build();
      return ReportProto.Report.newBuilder()
          .addAllRecords(
              report.facts().stream()
                  .map(ProtoToReportConverter::fromFact)
                  .collect(toImmutableList()))
          .setAttributionDestination(report.attributionDestination())
          .setAttributionReportTo(report.attributionReportTo())
          .setPrivacyBudgetKey(privacyBudgetKey)
          .setOriginalReportTime(report.originalReportTime().toEpochMilli())
          .build();
    }

    private static ReportProto.Report.Fact fromFact(Fact fact) {
      return ReportProto.Report.Fact.newBuilder().setKey(fact.key()).setValue(fact.value()).build();
    }
  }
}
