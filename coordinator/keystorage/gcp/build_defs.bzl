# Copyright 2024 Google LLC
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

load("@io_bazel_rules_docker//container:container.bzl", "container_image", "container_push")
load("//build_defs/shared:variables.bzl", "CLOUD_FUNCTIONS_JAVA_INVOKER_VERSION")

def keystorage_container(name, tag, registry, repository):
    container_name = name + "_container"
    container_image(
        name = container_name,
        base = "@java_base//image",
        cmd = [
            "/java-function-invoker-{}.jar".format(CLOUD_FUNCTIONS_JAVA_INVOKER_VERSION),
            "--classpath",
            "/KeyStorageServiceHttpCloudFunction_deploy.jar",
            "--target",
            "com.google.scp.coordinator.keymanagement.keystorage.service.gcp.KeyStorageServiceHttpFunction",
        ],
        files = [
            "//java/com/google/scp/coordinator/keymanagement/keystorage/service/gcp:KeyStorageServiceHttpCloudFunction_deploy.jar",
            "@maven//:com_google_cloud_functions_invoker_java_function_invoker",
        ],
    )

    container_push(
        name = name,
        format = "Docker",
        image = ":%s" % container_name,
        registry = registry,
        repository = repository,
        tag = tag,
    )
