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

package(default_visibility = ["//visibility:public"])

java_test(
    name = "LocalFileJobClientTest",
    srcs = ["LocalFileJobClientTest.java"],
    deps = [
        "//java/com/google/scp/operator/cpio/jobclient:aws",
        "//java/com/google/scp/operator/cpio/jobclient:model",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/shared/mapper",
        "//java/com/google/scp/shared/proto",
        "//java/external:acai",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//java/external:jackson_databind",
        "//java/external:jimfs",
        "//java/external:protobuf_java_util",
        "//operator/protos/shared/backend:java_proto",
    ],
)