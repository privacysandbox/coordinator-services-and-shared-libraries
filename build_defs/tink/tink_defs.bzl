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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Agu 10, 2022
# Commit for v1.7.0
TINK_VERSION = "1.7.0"
TINK_STRIP_PREFIX = "tink-{}".format(TINK_VERSION)
TINK_SHA256 = "ff272c968827ce06b262767934dc56ab520caa357a4747fc4a885b1cc711222f"
TINK_URLS = ["https://github.com/google/tink/archive/refs/tags/v1.7.0.zip"]

# List of Maven dependencies necessary for Tink to compile -- to be included in
# the list of Maven dependenceis passed to maven_install by the workspace.
TINK_MAVEN_ARTIFACTS = [
    "com.google.crypto.tink:tink:" + TINK_VERSION,
    "com.google.crypto.tink:tink-gcpkms:" + TINK_VERSION,
]

def import_tink_git(repo_name = ""):
    """
    Imports two of the Tink Bazel workspaces, @tink_base and @tink_java, from
    GitHub in order to get the latest version and apply any local patches for
    testing.

    Args:
      repo_name: name of the repo to import locally referenced files from
        (e.g. "@adm_cloud_scp"), needed when importing from another repo.
        TODO: find an alternative to specifying repo_name
        (e.g. by defining a top level repo name and using repo_mapping)
    """

    # Must be present to use Tink BUILD files which contain Android build rules.
    maybe(
        http_archive,
        name = "build_bazel_rules_android",
        urls = ["https://github.com/bazelbuild/rules_android/archive/v0.1.1.zip"],
        sha256 = "cd06d15dd8bb59926e4d65f9003bfc20f9da4b2519985c27e190cddc8b7a7806",
        strip_prefix = "rules_android-0.1.1",
    )

    # Tink contains multiple Bazel Workspaces. The "tink_java" workspace is what's
    # needed but it references the "tink_base" workspace so import both here.
    maybe(
        http_archive,
        name = "tink_base",
        sha256 = TINK_SHA256,
        strip_prefix = TINK_STRIP_PREFIX,
        urls = TINK_URLS,
    )

    # Note: loading and invoking `tink_java_deps` causes a cyclical dependency issue
    # so Tink's dependencies are just included directly in this workspace above.

    # Needed by Tink for JsonKeysetRead.
    maybe(
        http_archive,
        name = "rapidjson",
        build_file = Label("//build_defs/cc/shared/build_targets:rapidjson.BUILD"),
        sha256 = "30bd2c428216e50400d493b38ca33a25efb1dd65f79dfc614ab0c957a3ac2c28",
        strip_prefix = "rapidjson-418331e99f859f00bdc8306f69eba67e8693c55e",
        urls = [
            "https://github.com/miloyip/rapidjson/archive/418331e99f859f00bdc8306f69eba67e8693c55e.tar.gz",
        ],
    )

    maybe(
        http_archive,
        name = "tink_cc",
        sha256 = TINK_SHA256,
        strip_prefix = "{}/cc".format(TINK_STRIP_PREFIX),
        urls = TINK_URLS,
    )
