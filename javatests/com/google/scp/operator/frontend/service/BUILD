# Copyright 2022 Google LLC
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

load("@rules_java//java:defs.bzl", "java_library", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "FrontendServiceImplTest",
    srcs = ["FrontendServiceImplTest.java"],
    deps = [
        ":service",
        "//java/com/google/scp/operator/frontend/injection/modules/testing:fake_frontend_module",
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/converter",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/frontend/tasks",
        "//java/com/google/scp/operator/frontend/tasks/validation",
        "//java/com/google/scp/operator/frontend/testing:fake_request_info_validator",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:fake_metadata_db",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:job_generator",
        "//java/com/google/scp/operator/shared/injection/modules:aws_clients_module",
        "//java/com/google/scp/operator/shared/injection/modules/testing:fake_data_module",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/exception/testing",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/proto",
        "//java/external:acai",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//operator/protos/frontend/api/v1:java_proto",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/jobqueue:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

java_library(
    name = "service",
    srcs = ["ServiceJobGenerator.java"],
    deps = [
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/shared/model",
        "//java/external:acai",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//operator/protos/frontend/api/v1:java_proto",
        "//operator/protos/shared/backend:java_proto",
    ],
)
