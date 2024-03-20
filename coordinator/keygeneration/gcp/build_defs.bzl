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

load("@io_bazel_rules_docker//container:container.bzl", "container_image", "container_push")

def keygeneration_container(name, tag, registry, repository, log_policy_override = False):
    container_name = name + "_container"
    labels = {"tee.launch_policy.allow_cmd_override": "false"}
    if log_policy_override:
        labels["tee.launch_policy.log_redirect"] = "always"
    container_image(
        name = container_name,
        base = "@java_base//image",
        cmd = [
            "KeyGenerationApp_deploy.jar",
            "--multiparty",
        ],
        files = [Label("//java/com/google/scp/coordinator/keymanagement/keygeneration/app/gcp:KeyGenerationApp_deploy.jar")],
        labels = labels,
    )

    container_push(
        name = name,
        format = "Docker",
        image = ":%s" % container_name,
        registry = registry,
        repository = repository,
        tag = tag,
    )
