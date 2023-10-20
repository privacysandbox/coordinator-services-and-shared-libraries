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
load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "key_generation",
    srcs = glob(["*.java"]),
    deps = [
        "//java/com/google/scp/coordinator/clients/configclient/gcp",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/gcp/listener",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common/keyid",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/gcp",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/gcp",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/clients",
        "//java/com/google/scp/shared/clients/configclient",
        "//java/com/google/scp/shared/clients/configclient:gcp",
        "//java/com/google/scp/shared/gcp/util",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:gcp_core",
        "//java/external:gcp_kms",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jcommander",
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
    ],
)

java_binary(
    name = "KeyGenerationApp",
    main_class = "com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.KeyGenerationApplication",
    runtime_deps = [
        ":key_generation",
        "//java/external:gcp-http-client-jackson2",
        "//java/external:slf4j_simple",
    ],
)

alias(
    name = "KeyGenerationAppDeploy",
    actual = ":KeyGenerationApp_deploy.jar",
)

pkg_files(
    name = "KeyGenerationAppDeployPackage",
    srcs = [
        ":KeyGenerationAppDeploy",
    ],
    prefix = "jars",
    strip_prefix = ".",
)
