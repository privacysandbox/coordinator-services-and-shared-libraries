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

environment = "<environment name>"
project_id  = "<project id>"
# wipp_project_id_override = <project_id_to_deploy_wip_wipp> # defaults to `project_id` if not set
# wip_allowed_service_account_project_id_override = <project_id_to_create_wip_allowed_service_account> # defaults to `project_id` if not set
primary_region   = "us-central1"
secondary_region = "us-west3"

encryption_key_service_cloudfunction_memory_mb = 1024
key_storage_service_cloudfunction_memory_mb    = 1024

encryption_key_service_zip = "../../../jars/EncryptionKeyServiceHttpCloudFunctionDeploy.zip"
key_storage_service_zip    = "../../../jars/KeyStorageServiceHttpCloudFunctionDeploy.zip"

enable_domain_management   = true
parent_domain_name         = "<domain name from domainrecordsetup>"
parent_domain_name_project = "<project_id from domainrecordsetup>"

# Multi region location
# https://cloud.google.com/storage/docs/locations
mpkhs_package_bucket_location = "US"

# Note: Multi region can optionally be used but is roughly 4x the cost.
# nam10 is North America - uscentral1 and uswest3:
# https://cloud.google.com/spanner/docs/instance-configurations#configs-multi-region
spanner_instance_config  = "nam10"
spanner_processing_units = 100

# Uncomment and re-apply once primary MPKHS has been deployed.
#allowed_wip_iam_principals = ["serviceAccount:<output from mpkhs-primary in primary coordinator>"]

allowed_operator_user_group = "<output from allowedoperatorgroup in secondary coordinator>"

assertion_tee_swname = "CONFIDENTIAL_SPACE"

assertion_tee_container_image_hash_list = ["<list of hash values for images>"]

alarms_enabled            = true
alarms_notification_email = "<email to receive alarms>"
