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

exports_files([
    "kmstool.BUILD",
    "kmstool.patch",
])

genrule(
    name = "build_kmstool_dependencies",
    srcs = [
        "@aws_nitro_enclaves_sdk_c//:source_code_tar",
    ],
    outs = [
        "kmstool_enclave_cli",
        "libnsm.so",
    ],
    cmd_bash = """
    set -x
    SOURCE_TAR_FILE=$(location @aws_nitro_enclaves_sdk_c//:source_code_tar)
    tar -xf $$SOURCE_TAR_FILE -C $${SOURCE_TAR_FILE%/*}
    ROOT_DIR=$${SOURCE_TAR_FILE%/*}/aws_nitro_enclaves_sdk_c
    DOCKER_FILE=$$ROOT_DIR/containers/Dockerfile.al2
    docker build --target kmstool-enclave-cli -t kmstool-enclave-cli -f $$DOCKER_FILE $$ROOT_DIR
    CONTAINER_ID=$$(docker create kmstool-enclave-cli)
    docker cp $$CONTAINER_ID:/kmstool_enclave_cli $(location :kmstool_enclave_cli)
    docker cp $$CONTAINER_ID:/usr/lib64/libnsm.so $(location :libnsm.so)
    docker rm $$CONTAINER_ID
    """,
    tags = ["manual"],
)
