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

# Description:
#   Nitro Secure Module for Nitro Enclaves with attestation capability.

load("@rules_foreign_cc//foreign_cc:defs.bzl", "make")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

# This make rule do the following things:
# 1. Call "make .build-$(uname -m)-stable".
#    This target uses docker to build an image (nsm-api-stable) with rust and
#    cargo environment which are needed for building this library.
# 2. Spin up a container use the image in step 1 and run the cargo build command
#    inside the container. This step generates header and library files.
# 3. Commit the changes in step 2 back to the image.
# 4. Create a new container from new image in step 3 and copy header and library
#    files to host.
# 5. Remove the container and image.
# 6. The make rule now can find the header file in include dir and library file
#    in lib dir, so it can link them together and build.
make(
    name = "aws_nitro_enclaves_nsm_api",
    lib_source = ":all_srcs",
    out_shared_libs = [
        "libnsm.so",
    ],
    postfix_script = "IMAGE_NAME_TAG=\"nsm-api-stable-$(date +\"%Y%m%d%H%M%S\")\"; \
                      docker image tag nsm-api-stable $IMAGE_NAME_TAG; \
                      CONTAINER_TAG=\"container-$IMAGE_NAME_TAG\"; \
                      docker run --name $CONTAINER_TAG $IMAGE_NAME_TAG /bin/bash -c \"cargo build --release -p nsm-lib\"; \
                      docker cp $CONTAINER_TAG:/build/target/release/nsm.h $INSTALLDIR/include/nsm.h; \
                      docker cp $CONTAINER_TAG:/build/target/release/libnsm.so $INSTALLDIR/lib/libnsm.so; \
                      docker rm -f $CONTAINER_TAG; \
                      docker image rm $IMAGE_NAME_TAG; \
                      docker image rm nsm-api-stable",
    targets = [".build-$(uname -m)-stable"],
)
