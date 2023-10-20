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

load("@rules_java//java:defs.bzl", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "HttpPrivateKeyFetchingServiceTest",
    srcs = ["HttpPrivateKeyFetchingServiceTest.java"],
    deps = [
        "//java/com/google/scp/operator/cpio/cryptoclient",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/external:acai",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:mockito",
    ],
)

java_test(
    name = "HttpEncryptionKeyFetchingServiceTest",
    srcs = ["HttpEncryptionKeyFetchingServiceTest.java"],
    deps = [
        "//java/com/google/scp/operator/cpio/cryptoclient",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/external:acai",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:mockito",
    ],
)

java_test(
    name = "DecryptionKeyServiceImplTest",
    srcs = ["DecryptionKeyServiceImplTest.java"],
    deps = [
        "//java/com/google/scp/operator/cpio/cryptoclient",
        "//java/com/google/scp/shared/api/model",
        "//java/external:acai",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//java/external:tink",
        "//javatests/com/google/scp/shared/testutils/crypto:tink_utils",
    ],
)

java_test(
    name = "MultiPartyDecryptionKeyServiceImplTest",
    srcs = ["MultiPartyDecryptionKeyServiceImplTest.java"],
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//java/com/google/scp/operator/cpio/cryptoclient",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/crypto/tink",
        "//java/com/google/scp/shared/util",
        "//java/external:acai",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//java/external:tink",
        "//javatests/com/google/scp/shared/testutils/crypto:tink_utils",
    ],
)
