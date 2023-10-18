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

load(":sdk_image.bzl", "sdk_image")

LICENSES_TARGET = Label("//licenses:licenses_tar")

def sdk_container(
        *,
        name,
        sdk_binaries = {},
        platform,
        inside_tee,
        client_binaries = {},
        is_test_server = False,
        recover_client_binaries = True,
        recover_sdk_binaries = True,
        additional_env_variables = {},
        additional_files = [],
        additional_tars = [],
        pkgs_to_install = [],
        sdk_cmd_override = []):
    container_name = "%s_container" % name

    reproducible_container_name = "%s_reproducible_container" % name
    sdk_image(
        name = container_name,
        sdk_binaries = sdk_binaries,
        client_binaries = client_binaries,
        pkgs_to_install = [
            "ca-certificates",
            "curl",
            "rsyslog",
            "libatomic1",
            "libxml2-dev",
            "netbase",
            "libjemalloc-dev",
            "default-jre",
            "default-jdk",
        ] + pkgs_to_install,
        additional_env_variables = additional_env_variables,
        additional_files = additional_files,
        additional_tars = additional_tars,
        sdk_cmd_override = sdk_cmd_override,
        platform = platform,
        inside_tee = inside_tee,
        recover_client_binaries = recover_client_binaries,
        recover_sdk_binaries = recover_sdk_binaries,
    )

    # This rule can be used to build the container image in a reproducible manner.
    # It builds the image within a container with fixed libraries and dependencies.
    native.genrule(
        name = reproducible_container_name,
        srcs = [
            Label("//cc/public/tools:build_reproducible_container_image.sh"),
            Label("//:source_code_tar"),
            Label("//cc/tools/build:prebuilt_cc_build_container_image.tar"),
        ],
        outs = ["%s.tar" % reproducible_container_name],
        # NOTE: This order matters
        # Arguments:
        # $1 is the output tar, that is, the path where this rule generates its output ($@)
        # $2 is the packaged SCP source code ($(location //:source_code_tar))
        # $3 is the build container image tag
        # $4 is the name of the container to be built
        # $5 is the build container target path
        # $6+ are the build args
        cmd = "./$(location //cc/public/tools:build_reproducible_container_image.sh) $@ $(location //:source_code_tar)  $(location //cc/tools/build:prebuilt_cc_build_container_image.tar) %s %s %s %s %s" % (container_name, "%s:%s.tar" % (native.package_name(), container_name), "--//cc/cpio/server/interface:is_test_server=" + str(is_test_server), "--//cc/public/cpio/interface:platform=" + platform, "--//cc/public/cpio/interface:run_inside_tee=" + str(inside_tee)),
        tags = ["manual"],
    )
