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

environment      = "<environment name>"
project_id       = "<project id>"
primary_region   = "us-central1"
secondary_region = "us-west3"

enable_domain_management   = true
parent_domain_name         = "<domain name from domainrecordsetup>"
parent_domain_name_project = "<project_id from domainrecordsetup>"

get_public_key_cloudfunction_memory_mb         = 1024
encryption_key_service_cloudfunction_memory_mb = 1024

get_public_key_service_zip = "../../../jars/PublicKeyServiceHttpCloudFunctionDeploy.zip"
encryption_key_service_zip = "../../../jars/EncryptionKeyServiceHttpCloudFunctionDeploy.zip"

# Multi region location
# https://cloud.google.com/storage/docs/locations
mpkhs_package_bucket_location = "US"

# Note: Multi region can be used but is roughly 4x the cost.
# nam10 is North America - uscentral1 and uswest3:
# https://cloud.google.com/spanner/docs/instance-configurations#configs-multi-region
spanner_instance_config = "nam10"

# Spanner's compute capacity. 1000 processing units = 1 node and must be set as a multiple of 100.
spanner_processing_units = 100

# Uncomment line below for Protected Auction Key Service
#key_id_type="SEQUENCE"

# Set to "aggregation-service" or "protected-auction" depending on use-case
application_name = "<aggregation-service|protected-auction>"

key_generation_tee_allowed_sa = "<output from mpkhs_secondary in secondary coordinator>"

key_storage_service_base_url = "<output from key_storage_base_url in secondary coordinator>"

key_storage_service_cloudfunction_url = "<output from key_storage_cloudfunction_url in secondary coordinator>"

peer_coordinator_kms_key_uri = "<output from key_encryption_key_id in secondary coordinator>"

peer_coordinator_service_account = "<output from wip_verified_service_account in secondary coordinator>"

peer_coordinator_wip_provider = "<output from workload_identity_pool_provider_name in secondary coordinator>"

allowed_operator_user_group = "<output from allowedoperatorgroup in primary coordinator>"

alarms_enabled            = true
alarms_notification_email = "<email to receive alarms>"
