# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_java//java:defs.bzl", "java_binary", "java_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "avro_report",
    srcs = [
        "AvroReadExceptionChecker.java",
        "AvroReportRecord.java",
        "AvroReportWriter.java",
        "AvroReportWriterFactory.java",
        "AvroReportsReader.java",
        "AvroReportsReaderFactory.java",
    ],
    deps = [
        ":avro_reports_schema_supplier",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:avro",
        "//java/external:guava",
        "//java/external:javax_inject",
    ],
)

java_library(
    name = "avro_reports_schema_supplier",
    srcs = ["AvroReportsSchemaSupplier.java"],
    resources = [
        "//operator/protocol/avro:reports.avsc",
    ],
    deps = [
        ":avro_schema_supplier",
        "//java/external:javax_inject",
    ],
)

java_library(
    name = "avro_results_schema_supplier",
    srcs = ["AvroResultsSchemaSupplier.java"],
    resources = [
        "//operator/protocol/avro:results.avsc",
    ],
    deps = [
        ":avro_schema_supplier",
        "//java/external:javax_inject",
    ],
)

java_library(
    name = "avro_schema_supplier",
    srcs = ["AvroSchemaSupplier.java"],
    deps = [
        "//java/external:avro",
        "//java/external:guava",
    ],
)

java_binary(
    name = "AvroResultsDeserializerRunner",
    main_class = "com.google.scp.protocol.avro.AvroResultsDeserializerRunner",
    runtime_deps = [
        ":avro_result_deserializer_runner",
    ],
)

alias(
    name = "AvroResultsDeserializerRunnerDeploy",
    actual = ":AvroResultsDeserializerRunner_deploy.jar",
)

java_library(
    name = "avro_result_deserializer_runner",
    srcs = [
        "AvroResultsDeserializerRunner.java",
    ],
    deps = [
        ":avro_results_schema_supplier",
        "//java/com/google/scp/operator/worker/model",
        "//java/com/google/scp/operator/worker/testing:avro_results_file_reader",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jcommander",
    ],
)
