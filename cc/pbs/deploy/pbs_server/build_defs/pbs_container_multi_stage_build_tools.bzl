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

load("@io_bazel_rules_docker//container:container.bzl", "container_image")
load("@io_bazel_rules_docker//docker/package_managers:download_pkgs.bzl", "download_pkgs")
load("@io_bazel_rules_docker//docker/package_managers:install_pkgs.bzl", "install_pkgs")
load("@io_bazel_rules_docker//docker/util:run.bzl", "container_run_and_commit_layer", "container_run_and_extract")
load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("//cc/process_launcher:helpers.bzl", "executable_struct_to_json_str")

def load_pbs_container_multi_stage_container_build_tools():
    pbs_packages_shared_objects_extract_commands = [
        "mkdir -p /extracted_files/lib/x86_64-linux-gnu/",
        "mkdir -p /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libz.so.1 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libz.so.1) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libatomic.so.1 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libatomic.so.1) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cd /extracted_files && tar -zcvf shared_objects.tar * && cp shared_objects.tar /",
    ]

    pbs_lite_container_runtime_dependencies_layer_commands = [
        "addgroup adm",
        "ln -s /busybox/sh /bin/sh",
        "cd / && tar -xf shared_objects.tar",
        "rm /shared_objects.tar",
    ]

    pkg_tar(
        name = "pbs_binary_tar",
        srcs = [
            "//cc/pbs/pbs_server/src:privacy_budget_service",
        ],
        include_runfiles = True,
        mode = "0755",
        ownername = "root.root",
        package_dir = "opt/google/pbs",
        tags = ["manual"],
    )

    download_pkgs(
        name = "apt_pkgs_download",
        image_tar = "@debian_11//image",
        packages = [
            "libatomic1",
            "logrotate",
            "rsyslog",
            "socat",
        ],
        tags = ["manual"],
    )

    install_pkgs(
        name = "apt_pkgs_install",
        image_tar = "@debian_11//image",
        installables_tar = ":apt_pkgs_download.tar",
        output_image_name = "apt_pkgs_install",
        tags = ["manual"],
    )

    # Gather shared object needed by PBS so that they can be copied to the runtime container
    container_run_and_extract(
        name = "pbs_packages_shared_objects_extract",
        commands = pbs_packages_shared_objects_extract_commands,
        extract_file = "/shared_objects.tar",
        image = ":apt_pkgs_install.tar",
        tags = ["manual"],
    )

    container_image(
        name = "pbs_lite_container_base_image",
        base = "@debian_12_runtime//image",
        files = [
            ":pbs_packages_shared_objects_extract/shared_objects.tar",
        ],
        tags = ["manual"],
    )

    #
    # Dynamically generate the multiple PBS container build targets.
    # These targets must have different names to avoid container image
    # replacement in the case where all targets are build, like CI runs.
    #

    pbs_container_platforms = [
        "gcp",
        "local",
    ]

    for platform in pbs_container_platforms:
        # Extract the dependencies and remove the tars
        container_run_and_commit_layer(
            name = "pbs_lite_container_runtime_dependencies_layer_" + platform,
            commands = pbs_lite_container_runtime_dependencies_layer_commands,
            docker_run_flags = [
                "--entrypoint=''",
                "--user root",
            ],
            image = "pbs_lite_container_base_image.tar",
            tags = ["manual"],
        )

    for platform in pbs_container_platforms:
        container_image(
            name = "pbs_cloud_run_container_" + platform,
            base = "@debian_12_runtime//image",
            cmd = "/opt/google/pbs/privacy_budget_service",
            entrypoint = None,
            layers = [":pbs_lite_container_runtime_dependencies_layer_" + platform],
            tags = ["manual"],
            tars = [
                ":pbs_binary_tar",
            ],
            user = "root",
        )
