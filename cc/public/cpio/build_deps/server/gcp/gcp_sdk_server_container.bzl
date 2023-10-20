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

load("@io_bazel_rules_docker//container:container.bzl", "container_push")
load("//cc/public/cpio/build_deps/server:cmrt_sdk.bzl", "cmrt_sdk")

def gcp_sdk_server_container(
        name,
        client_binaries,
        image_registry,
        image_repository,
        image_tag,
        inside_tee = True,
        recover_client_binaries = True,
        recover_sdk_binaries = True,
        job_service_configs = {},
        nosql_database_service_configs = {},
        private_key_service_configs = {},
        public_key_service_configs = {},
        queue_service_configs = {}):
    """
    Creates a runnable target for pubshing a GCP SDK image to gcloud.
    The image name is the given name, and the image will be pushed to the given
    image_repository in the given image_registry with the given image_tag.

    To push the image, `bazel run` the provided name of this target.
    """

    cmrt_sdk_name = "%s_cmrt_sdk" % name
    cmrt_sdk(
        name = cmrt_sdk_name,
        platform = "gcp",
        inside_tee = inside_tee,
        client_binaries = client_binaries,
        recover_client_binaries = recover_client_binaries,
        recover_sdk_binaries = recover_sdk_binaries,
        job_service_configs = job_service_configs,
        nosql_database_service_configs = nosql_database_service_configs,
        private_key_service_configs = private_key_service_configs,
        public_key_service_configs = public_key_service_configs,
        queue_service_configs = queue_service_configs,
    )

    # Push image to GCP
    reproducible_container_name = "%s_reproducible_container" % cmrt_sdk_name
    container_push(
        name = name,
        format = "Docker",
        image = ":%s.tar" % reproducible_container_name,
        registry = image_registry,
        repository = image_repository,
        tag = image_tag,
        tags = ["manual"],
    )
