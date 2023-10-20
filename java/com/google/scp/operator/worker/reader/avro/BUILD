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

load("@rules_java//java:defs.bzl", "java_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "avro",
    srcs = [
        "LocalNioPathAvroReaderFactory.java",
        "LocalNioPathAvroRecordReader.java",
    ],
    deps = [
        "//java/com/google/scp/operator/cpio/blobstorageclient",
        "//java/com/google/scp/operator/cpio/blobstorageclient:model",
        "//java/com/google/scp/operator/worker/model",
        "//java/com/google/scp/operator/worker/reader",
        "//java/com/google/scp/protocol/avro:avro_report",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:avro",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:javax_inject",
    ],
)
