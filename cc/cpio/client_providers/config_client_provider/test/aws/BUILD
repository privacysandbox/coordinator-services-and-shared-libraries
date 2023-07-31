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

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_library(
    name = "test_aws_config_client_provider_lib",
    srcs = [
        "test_aws_config_client_provider.cc",
        "test_aws_config_client_provider.h",
        "//cc/cpio/client_providers/config_client_provider/src:config_client_provider_srcs",
        "//cc/cpio/client_providers/interface:config_client_provider_interface.h",
    ],
    copts = [
        "-DTEST_CPIO=1",
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/global_cpio/test:test_global_cpio_lib",
        "//cc/cpio/client_providers/interface:type_def",
        "//cc/cpio/client_providers/parameter_client_provider/test/aws:test_aws_parameter_client_provider_lib",
        "//cc/cpio/common/src/aws:aws_utils_lib",
        "//cc/cpio/common/test/aws:test_aws_utils_lib",
        "//cc/public/cpio/interface/config_client:type_def",
        "//cc/public/cpio/test/config_client:test_aws_config_client_options",
        "@aws_sdk_cpp//:ssm",
    ],
)
