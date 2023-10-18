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

load("@npm//@bazel/esbuild:index.bzl", "esbuild")

package(default_visibility = ["//visibility:public"])

esbuild(
    name = "adtech_onboarding_lambda_target",
    srcs = [
        "src/adtech_onboarding_lambda.ts",
        "util/db_operations.ts",
        "util/global_vars.ts",
        "util/models.ts",
        "util/role_operations.ts",
    ],
    entry_point = "src/adtech_onboarding_lambda.ts",
    output = "onboard.js",
    platform = "node",
    target = "es2020",
    deps = [
        "@npm//@aws-sdk/client-dynamodb",
        "@npm//@aws-sdk/client-iam",
        "@npm//@aws-sdk/lib-dynamodb",
        "@npm//@types/es6-promise",
        "@npm//typescript",
    ],
)

esbuild(
    name = "adtech_offboarding_lambda_target",
    srcs = [
        "src/adtech_offboarding_lambda.ts",
        "util/db_operations.ts",
        "util/global_vars.ts",
        "util/models.ts",
        "util/role_operations.ts",
    ],
    entry_point = "src/adtech_offboarding_lambda.ts",
    output = "offboard.js",
    platform = "node",
    target = "es2020",
    deps = [
        "@npm//@aws-sdk/client-dynamodb",
        "@npm//@aws-sdk/client-iam",
        "@npm//@aws-sdk/lib-dynamodb",
        "@npm//@types/es6-promise",
        "@npm//typescript",
    ],
)

esbuild(
    name = "adtech_read_lambda_target",
    srcs = [
        "src/adtech_read_lambda.ts",
        "util/db_operations.ts",
        "util/global_vars.ts",
        "util/models.ts",
        "util/role_operations.ts",
    ],
    entry_point = "src/adtech_read_lambda.ts",
    output = "read.js",
    platform = "node",
    target = "es2020",
    deps = [
        "@npm//@aws-sdk/client-dynamodb",
        "@npm//@aws-sdk/client-iam",
        "@npm//@aws-sdk/lib-dynamodb",
        "@npm//@types/es6-promise",
        "@npm//typescript",
    ],
)
