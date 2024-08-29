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
load("@rules_pkg//:mappings.bzl", "pkg_filegroup")

package(default_visibility = ["//visibility:public"])

pkg_filegroup(
    name = "multipartykeyhosting_modules_pkg",
    srcs = [
        "//coordinator/terraform/gcp/modules/allowedusergroup",
        "//coordinator/terraform/gcp/modules/bazel",
        "//coordinator/terraform/gcp/modules/cloud-nat",
        "//coordinator/terraform/gcp/modules/domain_a_records",
        "//coordinator/terraform/gcp/modules/encryptionkeyservice",
        "//coordinator/terraform/gcp/modules/keydb",
        "//coordinator/terraform/gcp/modules/keygenerationservice",
        "//coordinator/terraform/gcp/modules/keystorageservice",
        "//coordinator/terraform/gcp/modules/parameters",
        "//coordinator/terraform/gcp/modules/publickeyhostingservice",
        "//coordinator/terraform/gcp/modules/version",
        "//coordinator/terraform/gcp/modules/vpc",
        "//coordinator/terraform/gcp/modules/workloadidentitypoolprovider",
    ],
    prefix = "modules",
)

pkg_filegroup(
    name = "multipartykeyhosting_modules_shared_pkg",
    srcs = [
        "//coordinator/terraform/gcp/modules/shared/cloudfunction_alarms",
        "//coordinator/terraform/gcp/modules/shared/cloudfunction_dashboards",
        "//coordinator/terraform/gcp/modules/shared/loadbalancer_alarms",
    ],
    prefix = "modules/shared",
)

pkg_filegroup(
    name = "distributedpbs_modules_pkg",
    srcs = [
        "//coordinator/terraform/gcp/modules/bazel",
        "//coordinator/terraform/gcp/modules/distributedpbs_artifact_registry",
        "//coordinator/terraform/gcp/modules/distributedpbs_auth_db",
        "//coordinator/terraform/gcp/modules/distributedpbs_auth_service",
        "//coordinator/terraform/gcp/modules/distributedpbs_domain_a_record",
        "//coordinator/terraform/gcp/modules/distributedpbs_load_balancer",
        "//coordinator/terraform/gcp/modules/distributedpbs_managed_instance_group_environment",
        "//coordinator/terraform/gcp/modules/distributedpbs_managed_instance_group_environment/files",
        "//coordinator/terraform/gcp/modules/distributedpbs_service_account",
        "//coordinator/terraform/gcp/modules/distributedpbs_storage",
    ],
    prefix = "modules",
)
