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

environment      = "<environment>"
primary_region   = "us-east-1"
secondary_region = "us-west-2"

api_version = "v1alpha"

# Alarm names are created with the following format:
# $Criticality$Alertname$CustomAlarmLabel
custom_alarm_label = "<CustomAlarmLabel>"

## Uncomment if not set in ami_params.auto.tfvars
# key_generation_ami_name_prefix = "<ami name prefix>"
# key_generation_ami_owners = ["<ami owner account id>"]

keystorage_api_base_url       = "<output from mpkhs_secondary>"
coordinator_b_assume_role_arn = "<output from mpkhs_secondary>"

## Uncomment below config values for development environments
## BEGIN - development default configuration values
# get_public_key_lambda_provisioned_concurrency_enabled = false
# get_public_key_lambda_memory_mb = 3000
## END - development default configuration values

alarms_enabled           = false
alarm_notification_email = "<notification email>"

enable_domain_management = false
# must be false on first apply
enable_dynamodb_replica = false
