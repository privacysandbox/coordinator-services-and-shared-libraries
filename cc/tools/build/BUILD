# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@io_bazel_rules_docker//container:container.bzl", "container_image")
load("@io_bazel_rules_docker//docker/package_managers:download_pkgs.bzl", "download_pkgs")
load("@io_bazel_rules_docker//docker/package_managers:install_pkgs.bzl", "install_pkgs")
load("@io_bazel_rules_docker//docker/util:run.bzl", "container_run_and_commit", "container_run_and_commit_layer")
load("@rules_pkg//:pkg.bzl", "pkg_tar")

package(default_visibility = ["//visibility:public"])

exports_files(["build_container_params.bzl"])

# Please do not remove any of the manual tags below.
# These bazel targets take long time to build and we only build them when modifying container.

download_pkgs(
    name = "cc_build_apt_pkgs_download",
    image_tar = "@debian_11//image",
    packages = [
        "autoconf",
        "build-essential",
        "ca-certificates",
        "clang-13",
        "cmake",
        "curl",
        "docker.io",
        "git",
        "golang",
        "libcurl4-openssl-dev",
        "lld",
        "make",
        "ninja-build",
        "openjdk-11-jre",
        "protobuf-compiler",
        "python-is-python3",
        "python3",
        "python3-distutils",
        "python3-pip",
        "rsyslog",
        "socat",
        "sudo",
        "tar",
        "zlib1g-dev",
    ],
    tags = ["manual"],
)

install_pkgs(
    name = "cc_build_apt_pkgs_install",
    image_tar = "@debian_11//image",
    installables_tar = ":cc_build_apt_pkgs_download.tar",
    output_image_name = "cc_build_apt_pkgs_install",
    tags = ["manual"],
)

container_image(
    name = "container_to_build_cc_base",
    base = "cc_build_apt_pkgs_install.tar",
    cmd = "/bin/bash -c sleep infinity;",
    tags = ["manual"],
)

# NOTE: This bazel version should be kept up to date with the glinux version
container_run_and_commit_layer(
    name = "container_to_build_cc_bazel",
    commands = [
        "curl -L https://github.com/bazelbuild/bazelisk/releases/download/v1.12.0/bazelisk-linux-amd64 -o /usr/bin/bazel",
        "chmod +x /usr/bin/bazel",
        "ln -s /usr/bin/clang-13 /usr/bin/clang",
        "ln -s /usr/bin/clang++-13 /usr/bin/clang++",
        "ln -s /usr/bin/clang-cpp-13 /usr/bin/clang-cpp",
        "pip3 install \"libclang~=13.0\"",
    ],
    env = {
        "CC": "/usr/bin/clang",
        "CXX": "/usr/bin/clang++",
    },
    image = ":cc_build_apt_pkgs_install.tar",
    tags = ["manual"],
)

container_run_and_commit_layer(
    name = "container_to_build_cc_boringssl",
    commands = [
        "mkdir /tmp/build",
        "cd /tmp/build",
        "git clone https://github.com/google/boringssl.git boringssl",
        "cd boringssl",
        "git checkout 64393b57e8734b92a6ba784bcfc02b1aa01e5ff2",
        "mkdir build",
        "cd build",
        "cmake .. -DCMAKE_PREFIX_PATH=/opt/deps -DCMAKE_INSTALL_PREFIX=/opt/deps",
        "make install",
        "rm -rf /tmp/build",
    ],
    image = ":cc_build_apt_pkgs_install.tar",
    tags = ["manual"],
)

container_run_and_commit_layer(
    name = "container_to_build_cc_aws_c_common",
    commands = [
        "mkdir /tmp/build",
        "cd /tmp/build",
        "git clone -b v0.8.23 https://github.com/awslabs/aws-c-common.git",
        "cmake -DCMAKE_PREFIX_PATH=/opt/deps -DBUILD_TESTING=0 -DCMAKE_INSTALL_PREFIX=/opt/deps -GNinja -S aws-c-common -B aws-c-common/build",
        "cmake --build aws-c-common/build --target install",
        "rm -rf /tmp/build",
    ],
    image = ":cc_build_apt_pkgs_install.tar",
    tags = ["manual"],
)

pkg_tar(
    name = "enclaves-patch",
    srcs = [
        "//build_defs/cc/aws:aws_c_io.patch",
        "//build_defs/cc/aws:aws_nitro_enclaves_sdk.patch",
    ],
    mode = "0644",
    package_dir = "/opt/deps/",
    tags = ["manual"],
)

container_image(
    name = "container_to_build_cc_shared_libraries",
    base = "container_to_build_cc_base.tar",
    layers = [
        ":container_to_build_cc_bazel",
        ":container_to_build_cc_boringssl",
        ":container_to_build_cc_aws_c_common",
    ],
    tags = ["manual"],
    tars = [":enclaves-patch"],
)

container_run_and_commit_layer(
    name = "container_to_build_cc_aws_sdk",
    commands = [
        "mkdir /tmp/aws",
        "cd /tmp/aws",
        "git clone -b v0.1.5 https://github.com/awslabs/aws-checksums.git",
        "cmake -DCMAKE_PREFIX_PATH=/opt/deps -DBUILD_TESTING=0 -DCMAKE_INSTALL_PREFIX=/opt/deps -GNinja -S aws-checksums -B aws-checksums/build",
        "cmake --build aws-checksums/build --parallel $(nproc) --target install",
        "git clone -b v0.1.5 https://github.com/awslabs/aws-c-event-stream.git",
        "cmake -DCMAKE_PREFIX_PATH=/opt/deps -DBUILD_TESTING=0 -DCMAKE_INSTALL_PREFIX=/opt/deps -GNinja -S aws-c-event-stream -B aws-c-event-stream/build",
        "cmake --build aws-c-event-stream/build --parallel $(nproc) --target install",
        "git clone --branch 1.8.186 https://github.com/aws/aws-sdk-cpp.git aws-sdk-cpp",
        "cd aws-sdk-cpp",
        "mkdir build",
        "cd build",
        "cmake .. -DCMAKE_PREFIX_PATH=/opt/deps -DCMAKE_INSTALL_PREFIX=/opt/deps -DCMAKE_BUILD_TYPE=Release " +
        "-DBUILD_DEPS=OFF -DBUILD_SHARED_LIBS=OFF -DENABLE_TESTING=OFF " +
        '-DBUILD_ONLY=\\"kms;monitoring;events;logs;ec2;sts;ssm;s3;dynamodb;sqs;autoscaling\\" ' +
        "-DENABLE_OPENSSL_ENCRYPTION=ON -DENABLE_CURL_CLIENT=ON -DPLATFORM_LINUX=ON",
        "make -j $(nproc)",
        "make install",
        "cd /tmp",
        "rm -rf aws",
    ],
    image = ":container_to_build_cc_shared_libraries.tar",
    tags = ["manual"],
)

container_run_and_commit_layer(
    name = "container_to_build_cc_enclaves",
    commands = [
        "curl https://sh.rustup.rs -sSf | sh -s -- -y --profile minimal --default-toolchain 1.63",
        "mkdir /tmp/crt-builder",
        "cd /tmp/crt-builder",
        "git clone --depth 1 -b v1.3.46 https://github.com/aws/s2n-tls.git",
        "cmake -DCMAKE_PREFIX_PATH=/opt/deps -DBUILD_TESTING=0 -DCMAKE_INSTALL_PREFIX=/opt/deps -S s2n-tls -B s2n-tls/build",
        "cmake --build s2n-tls/build --parallel $(nproc) --target install",
        "git clone --depth 1 -b v0.1.11 https://github.com/awslabs/aws-c-sdkutils.git",
        "cmake -DCMAKE_PREFIX_PATH=/opt/deps -DBUILD_TESTING=0 -DCMAKE_INSTALL_PREFIX=/opt/deps -GNinja -S aws-c-sdkutils -B aws-c-sdkutils/build",
        "cmake --build aws-c-sdkutils/build --parallel $(nproc) --target install",
        "git clone --depth 1 -b v0.5.27 https://github.com/awslabs/aws-c-cal.git",
        "cmake -DCMAKE_PREFIX_PATH=/opt/deps -DBUILD_TESTING=0 -DCMAKE_INSTALL_PREFIX=/opt/deps -DUSE_OPENSSL=0 -DOPENSSL_IS_AWSLC=0 -GNinja -S aws-c-cal -B aws-c-cal/build",
        "cmake --build aws-c-cal/build --parallel $(nproc) --target install",
        "git clone --depth 1 -b v0.13.26 https://github.com/awslabs/aws-c-io.git",
        "cd aws-c-io",
        "git apply /opt/deps/aws_c_io.patch",
        "cd /tmp/crt-builder",
        "cmake -DUSE_VSOCK=1 -DCMAKE_PREFIX_PATH=/opt/deps -DBUILD_TESTING=0 -DCMAKE_INSTALL_PREFIX=/opt/deps -GNinja -S aws-c-io -B aws-c-io/build",
        "cmake --build aws-c-io/build --parallel $(nproc) --target install",
        "git clone --depth 1 -b v0.2.17 http://github.com/awslabs/aws-c-compression.git",
        "cmake -DCMAKE_PREFIX_PATH=/opt/deps -DBUILD_TESTING=0 -DCMAKE_INSTALL_PREFIX=/opt/deps -GNinja -S aws-c-compression -B aws-c-compression/build",
        "cmake --build aws-c-compression/build --parallel $(nproc) --target install",
        "git clone --depth 1 -b v0.7.6 https://github.com/awslabs/aws-c-http.git",
        "cmake -DCMAKE_PREFIX_PATH=/opt/deps -DBUILD_TESTING=0 -DCMAKE_INSTALL_PREFIX=/opt/deps -GNinja -S aws-c-http -B aws-c-http/build",
        "cmake --build aws-c-http/build --parallel $(nproc) --target install",
        "git clone --depth 1 -b v0.7.0 https://github.com/awslabs/aws-c-auth.git",
        "cmake -DCMAKE_PREFIX_PATH=/opt/deps -DBUILD_TESTING=0 -DCMAKE_INSTALL_PREFIX=/opt/deps -GNinja -S aws-c-auth -B aws-c-auth/build",
        "cmake --build aws-c-auth/build --parallel $(nproc) --target install",
        "git clone --depth 1 -b json-c-0.16-20220414 https://github.com/json-c/json-c.git",
        "cmake -DCMAKE_PREFIX_PATH=/opt/deps -DBUILD_TESTING=0 -DCMAKE_INSTALL_PREFIX=/opt/deps -DBUILD_SHARED_LIBS=OFF -GNinja -S json-c -B json-c/build",
        "cmake --build json-c/build --parallel $(nproc)  --target install",
        "git clone --depth 1 -b v0.3.0 https://github.com/aws/aws-nitro-enclaves-nsm-api.git",
        "cd aws-nitro-enclaves-nsm-api",
        ". /root/.cargo/env && cargo build --release --jobs $(nproc) -p nsm-lib",
        "mv /tmp/crt-builder/aws-nitro-enclaves-nsm-api/target/release/libnsm.so /opt/deps/lib",
        "mv /tmp/crt-builder/aws-nitro-enclaves-nsm-api/target/release/nsm.h /opt/deps/include",
        "cd /tmp/crt-builder",
        "git clone --depth 1 -b v0.4.0 https://github.com/aws/aws-nitro-enclaves-sdk-c.git",
        "cd aws-nitro-enclaves-sdk-c",
        "git apply /opt/deps/aws_nitro_enclaves_sdk.patch",
        "cmake -DCMAKE_PREFIX_PATH=/opt/deps -DCMAKE_INSTALL_PREFIX=/opt/deps -DBUILD_SHARED_LIBS=OFF -GNinja -S . -B build",
        "cmake --build build --parallel $(nproc) --target install",
        "cd /tmp",
        "rm -rf crt-builder",
    ],
    image = ":container_to_build_cc_shared_libraries.tar",
    tags = ["manual"],
)

container_image(
    name = "container_to_build_cc_tar",
    base = "container_to_build_cc_shared_libraries.tar",
    cmd = "/bin/bash -c sleep infinity;",
    layers = [
        ":container_to_build_cc_aws_sdk",
        ":container_to_build_cc_enclaves",
    ],
    tags = ["manual"],
)

container_run_and_commit(
    name = "container_to_build_cc",
    commands = [""],
    image = "container_to_build_cc_tar.tar",
    tags = ["manual"],
)

# Load pre-built container image used for cc builds
# This image corresponds to the tar container_to_build_cc_commit.tar (output of rule container_to_build_cc)
# However, building this image every time is needed would take too long, so we build it,
# upload it to a registry and then just download it when needed.
# For additional info refer to: cc/tools/build/README.md
container_image(
    name = "prebuilt_cc_build_container_image",
    base = "@prebuilt_cc_build_container_image_pull//image",
    tags = ["manual"],
)
