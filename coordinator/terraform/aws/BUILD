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

load("@rules_pkg//:mappings.bzl", "pkg_attributes", "pkg_files")
load("@rules_pkg//:pkg.bzl", "pkg_tar", "pkg_zip")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "multiparty_dist_files",
    srcs = [
        "//cc/pbs/deploy/pbs_server/build_defs:reproducible_pbs_container_aws",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws:EncryptionKeyServiceLambdaDeploy",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/aws:PublicKeyApiGatewayHandlerDeploy",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/service/aws:KeyStorageServiceLambdaDeploy",
        "//java/com/google/scp/coordinator/pbsprober/service/aws:PrivacyBudgetProberHandlerLambdaDeploy",
        "//java/com/google/scp/coordinator/privacy/budgeting/service/aws:aws_consume_privacy_budget_handler_binary_deploy",
        "//python/privacybudget/aws/pbs_auth_handler:pbs_auth_handler_lambda_zipper",
    ],
    prefix = "dist",
)

pkg_files(
    name = "multiparty_dist_scripts",
    srcs = [
        "//coordinator/privacybudget/aws/deploy:deploy_distributedpbs",
    ],
    # These scripts are executable
    attributes = pkg_attributes(
        mode = "0755",
    ),
)

pkg_tar(
    name = "multiparty_coordinator_tar",
    srcs = [
        ":multiparty_dist_files",
        ":multiparty_dist_scripts",
        "//coordinator/terraform/aws/applications:distributed_pbs_application_pkg",
        "//coordinator/terraform/aws/applications:distributed_pbs_base_pkg",
        "//coordinator/terraform/aws/applications:domainrecords_module_pkg",
        "//coordinator/terraform/aws/applications:multipartykeyhosting_primary_module_pkg",
        "//coordinator/terraform/aws/applications:multipartykeyhosting_secondary_module_pkg",
        "//coordinator/terraform/aws/applications:roleprovider_module_pkg",
        "//coordinator/terraform/aws/environments_mp_primary/demo:mp_primary_coordinator_demo",
        "//coordinator/terraform/aws/environments_mp_primary/shared:mp_primary_coordinator_shared",
        "//coordinator/terraform/aws/environments_mp_primary/testing:mp_primary_coordinator_testing",
        "//coordinator/terraform/aws/environments_mp_secondary/demo:mp_secondary_coordinator_demo",
        "//coordinator/terraform/aws/environments_mp_secondary/shared:mp_secondary_coordinator_shared",
        "//coordinator/terraform/aws/environments_mp_secondary/testing:mp_secondary_coordinator_testing",
        "//coordinator/terraform/aws/modules:apigateway_pkg",
        "//coordinator/terraform/aws/modules:bazel_module_pkg",
        "//coordinator/terraform/aws/modules:distributed_pbs_access_policy_pkg",
        "//coordinator/terraform/aws/modules:distributed_pbs_alarms_pkg",
        "//coordinator/terraform/aws/modules:distributed_pbs_auth_db_pkg",
        "//coordinator/terraform/aws/modules:distributed_pbs_auth_records_pkg",
        "//coordinator/terraform/aws/modules:distributed_pbs_auth_service_pkg",
        "//coordinator/terraform/aws/modules:distributed_pbs_beanstalk_environment_files_pkg",
        "//coordinator/terraform/aws/modules:distributed_pbs_beanstalk_environment_pkg",
        "//coordinator/terraform/aws/modules:distributed_pbs_beanstalk_storage_pkg",
        "//coordinator/terraform/aws/modules:distributed_pbs_container_registry_pkg",
        "//coordinator/terraform/aws/modules:distributed_pbs_storage_pkg",
        "//coordinator/terraform/aws/modules:encryptionkey_service_pkg",
        "//coordinator/terraform/aws/modules:keydb_pkg",
        "//coordinator/terraform/aws/modules:keyjobqueue_pkg",
        "//coordinator/terraform/aws/modules:keymanagementroleprovider_pkg",
        "//coordinator/terraform/aws/modules:keystoragedashboard_pkg",
        "//coordinator/terraform/aws/modules:keystorageservice_pkg",
        "//coordinator/terraform/aws/modules:multipartykeygeneration_service_pkg",
        "//coordinator/terraform/aws/modules:privacybudgetdashboard_pkg",
        "//coordinator/terraform/aws/modules:privatekeydomainprovider_pkg",
        "//coordinator/terraform/aws/modules:publickeyhostingcloudfront_pkg",
        "//coordinator/terraform/aws/modules:publickeyhostingdashboard_pkg",
        "//coordinator/terraform/aws/modules:publickeyhostingservice_pkg",
        "//coordinator/terraform/aws/modules:shared_bucketprovider_pkg",
        "//coordinator/terraform/aws/modules:shared_lambda_roles_pkg",
        "//coordinator/terraform/aws/modules:shared_workerenclaveencryptionkey_pkg",
        "//coordinator/terraform/aws/modules:sslcertificateprovider_pkg",
        "//coordinator/terraform/aws/modules:unifiedkeyhostingdashboard_pkg",
        "//coordinator/terraform/aws/modules:vpc_v2_pkg",
        "//coordinator/terraform/aws/monitoring:common_api_gateway_alarms_pkg",
        "//coordinator/terraform/aws/monitoring:common_lambda_alarms_pkg",
        "//coordinator/terraform/aws/monitoring:common_mp_api_gateway_alarms_pkg",
        "//coordinator/terraform/aws/monitoring:common_sqs_queue_alarms_pkg",
        "//licenses",
    ],
    extension = "tgz",
    strip_prefix = "/",
)
