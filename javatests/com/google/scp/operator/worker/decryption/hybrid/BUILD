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

load("@rules_java//java:defs.bzl", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "HybridDecryptionCipherFactoryTest",
    srcs = ["HybridDecryptionCipherFactoryTest.java"],
    deps = [
        "//java/com/google/scp/operator/cpio/cryptoclient",
        "//java/com/google/scp/operator/worker/decryption",
        "//java/com/google/scp/operator/worker/decryption/hybrid",
        "//java/com/google/scp/operator/worker/model",
        "//java/external:acai",
        "//java/external:google_truth",
        "//java/external:guice",
    ],
)

java_test(
    name = "HybridDecryptionCipherTest",
    srcs = ["HybridDecryptionCipherTest.java"],
    deps = [
        "//java/com/google/scp/operator/worker/decryption/hybrid",
        "//java/external:google_truth",
        "//java/external:tink",
    ],
)

java_test(
    name = "HybridDeserializingReportDecrypterTest",
    srcs = ["HybridDeserializingReportDecrypterTest.java"],
    deps = [
        "//java/com/google/scp/operator/cpio/cryptoclient",
        "//java/com/google/scp/operator/worker/decryption",
        "//java/com/google/scp/operator/worker/decryption/hybrid",
        "//java/com/google/scp/operator/worker/model",
        "//java/com/google/scp/operator/worker/model/serdes",
        "//java/com/google/scp/operator/worker/model/serdes/proto:report_serdes",
        "//java/com/google/scp/operator/worker/testing:fake_report_generator",
        "//java/com/google/scp/simulation/encryption",
        "//java/com/google/scp/simulation/encryption/hybrid",
        "//java/external:acai",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:tink",
    ],
)
