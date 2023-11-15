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

load("@py3_privacybudget_aws_pbs_auth_handler_deps//:requirements.bzl", "requirement")
load("@rules_python//python:defs.bzl", "py_test")

package(default_visibility = ["//visibility:public"])

py_test(
    name = "auth_lambda_handler_test",
    size = "small",
    srcs = [
        "auth_lambda_handler.py",
        "auth_lambda_handler_test.py",
        "boto3.py",
    ],
    deps = [
        requirement("publicsuffixlist"),
    ],
)

filegroup(
    name = "auth_lambda_handler",
    srcs = ["auth_lambda_handler.py"],
    data = [requirement("publicsuffixlist")],
)

genrule(
    name = "pbs_auth_handler_lambda_zipper",
    srcs = [
        "//build-scripts/aws/pbs_auth_lambda:create_pbs_auth_handler_package.sh",
        "auth_lambda_handler.py",
        "requirements.txt",
    ],
    outs = ["pbs_auth_handler_lambda.zip"],
    cmd = "$(SRCS) $@",
)
