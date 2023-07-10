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

load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("@io_bazel_rules_docker//container:container.bzl", "container_image")
load("@io_bazel_rules_docker//docker/package_managers:download_pkgs.bzl", "download_pkgs")
load("@io_bazel_rules_docker//docker/package_managers:install_pkgs.bzl", "install_pkgs")
load("//cc/process_launcher:helpers.bzl", "executable_struct_to_json_str")
load("@io_bazel_rules_docker//docker/util:run.bzl", "container_run_and_commit_layer", "container_run_and_extract")

def load_pbs_container_multi_stage_container_build_tools():
    pbs_packages_socat_extract_commands = [
        "mkdir -p /extracted_files/usr/bin/",
        "mkdir -p /extracted_files/lib/x86_64-linux-gnu/",
        "mkdir -p /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/bin/socat /extracted_files/usr/bin/",
        "cp /usr/lib/x86_64-linux-gnu/libwrap.so.0 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libwrap.so.0) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libutil.so.1 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libutil.so.1) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libssl.so.1.1 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libcrypto.so.1.1 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libc.so.6 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libc.so.6) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libnsl.so.2 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libnsl.so.2) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libpthread.so.0 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libpthread.so.0) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libdl.so.2 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libdl.so.2) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libtirpc.so.3 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libtirpc.so.3) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libgssapi_krb5.so.2 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libgssapi_krb5.so.2) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libkrb5.so.3 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libkrb5.so.3) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libk5crypto.so.3 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libk5crypto.so.3) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libcom_err.so.2 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libcom_err.so.2) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libkrb5support.so.0 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libkrb5support.so.0) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libkeyutils.so.1 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libkeyutils.so.1) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libresolv.so.2 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libresolv.so.2) /extracted_files/lib/x86_64-linux-gnu/",
        "cd /extracted_files && tar -zcvf socat_and_dependencies.tar * && cp socat_and_dependencies.tar /",
    ]

    pbs_packages_rsyslog_extract_commands = [
        "mkdir -p /extracted_files/usr/sbin/",
        "mkdir -p /extracted_files/etc/",
        "mkdir -p /extracted_files/lib/x86_64-linux-gnu/",
        "mkdir -p /extracted_files/usr/lib/x86_64-linux-gnu/",
        "mkdir -p /extracted_files/usr/lib/x86_64-linux-gnu/rsyslog/",
        "cp /usr/sbin/rsyslogd /extracted_files/usr/sbin/",
        "cp /etc/rsyslog.conf /extracted_files/etc/",
        "cp /lib/x86_64-linux-gnu/libz.so.1 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libz.so.1) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libpthread.so.0 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libpthread.so.0) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libdl.so.2 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libdl.so.2) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libestr.so.0 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libestr.so.0) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libfastjson.so.4 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libfastjson.so.4) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libsystemd.so.0 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libsystemd.so.0) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libuuid.so.1 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libuuid.so.1) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libc.so.6 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libc.so.6) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/librt.so.1 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/librt.so.1) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/liblzma.so.5 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/liblzma.so.5) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libzstd.so.1 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libzstd.so.1) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/liblz4.so.1 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/liblz4.so.1) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libgcrypt.so.20 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libgcrypt.so.20) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libgpg-error.so.0 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libgpg-error.so.0) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/rsyslog/* /extracted_files/usr/lib/x86_64-linux-gnu/rsyslog/",
        "cd /extracted_files && tar -zcvf rsyslog_and_dependencies.tar * && cp rsyslog_and_dependencies.tar /",
    ]

    pbs_packages_shared_objects_extract_commands = [
        "mkdir -p /extracted_files/lib/x86_64-linux-gnu/",
        "mkdir -p /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/libz.so.1 /extracted_files/lib/x86_64-linux-gnu/",
        "cp /lib/x86_64-linux-gnu/$(readlink /lib/x86_64-linux-gnu/libz.so.1) /extracted_files/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/libatomic.so.1 /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cp /usr/lib/x86_64-linux-gnu/$(readlink /usr/lib/x86_64-linux-gnu/libatomic.so.1) /extracted_files/usr/lib/x86_64-linux-gnu/",
        "cd /extracted_files && tar -zcvf shared_objects.tar * && cp shared_objects.tar /",
    ]

    pbs_container_runtime_dependencies_layer_commands = [
        "addgroup adm",
        "ln -s /busybox/sh /bin/sh",
        "cd / && tar -xf shared_objects.tar",
        "rm /shared_objects.tar",
        "cd / && tar -xf rsyslog_and_dependencies.tar",
        "rm /rsyslog_and_dependencies.tar",
        "cd / && tar -xf socat_and_dependencies.tar",
        "rm /socat_and_dependencies.tar",
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

    pkg_tar(
        name = "process_launcher_tar",
        srcs = [
            "//cc/process_launcher:scp_process_launcher",
        ],
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

    # Gather socat and all the dependencies it needs to run into a tar so that it can be copied into the runtime container
    container_run_and_extract(
        name = "pbs_packages_socat_extract",
        commands = pbs_packages_socat_extract_commands,
        extract_file = "/socat_and_dependencies.tar",
        image = ":apt_pkgs_install.tar",
        tags = ["manual"],
    )

    # Gather rsyslog and all the dependencies it needs to run into a tar so that it can be copied into the runtime container
    container_run_and_extract(
        name = "pbs_packages_rsyslog_extract",
        commands = pbs_packages_rsyslog_extract_commands,
        extract_file = "/rsyslog_and_dependencies.tar",
        image = ":apt_pkgs_install.tar",
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

    # Base image to extract the dependencies in
    container_image(
        name = "pbs_container_base_image",
        base = "@debian_11_runtime//image",
        files = [
            ":pbs_packages_rsyslog_extract/rsyslog_and_dependencies.tar",
            ":pbs_packages_shared_objects_extract/shared_objects.tar",
            ":pbs_packages_socat_extract/socat_and_dependencies.tar",
        ],
        tags = ["manual"],
    )

    container_cmd = ["/opt/google/pbs/scp_process_launcher"]
    rsyslog_daemon = {
        "file_name": "/usr/sbin/rsyslogd",
        "args": [
            # Run in the foreground
            "-n",
        ],
    }
    pbs_executable = {
        "file_name": "/opt/google/pbs/privacy_budget_service",
        "args": [],
    }
    container_cmd.append(executable_struct_to_json_str(rsyslog_daemon))
    container_cmd.append(executable_struct_to_json_str(pbs_executable))

    #
    # Dynamically generate the multiple PBS container build targets.
    # These targets must have different names to avoid container image
    # replacement in the case where all targets are build, like CI runs.
    #

    pbs_container_platforms = [
        "aws",
        "aws_integration_test",
        "gcp",
        "gcp_integration_test",
    ]

    [
        # Extract the dependencies and remove the tars
        container_run_and_commit_layer(
            name = "pbs_container_runtime_dependencies_layer_" + platform,
            commands = pbs_container_runtime_dependencies_layer_commands,
            docker_run_flags = [
                "--entrypoint=''",
                "--user root",
            ],
            image = "pbs_container_base_image.tar",
            tags = ["manual"],
        )
        for platform in pbs_container_platforms
    ]

    [
        container_image(
            name = "pbs_container_" + platform,
            base = "@debian_11_runtime//image",
            cmd = container_cmd,
            entrypoint = None,
            layers = [":pbs_container_runtime_dependencies_layer_" + platform],
            tags = ["manual"],
            tars = [
                ":pbs_binary_tar",
                ":process_launcher_tar",
            ],
            user = "root",
        )
        for platform in pbs_container_platforms
    ]
