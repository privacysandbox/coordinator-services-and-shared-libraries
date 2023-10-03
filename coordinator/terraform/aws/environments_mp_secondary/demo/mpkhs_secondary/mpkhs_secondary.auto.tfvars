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

environment = "<environment>"
region      = "us-east-1"

api_version = "v1alpha"

alarms_enabled           = false
alarm_notification_email = "<notification email>"
# Alarm names are created with the following format:
# $Criticality$Alertname$CustomAlarmLabel
custom_alarm_label = "<CustomAlarmLabel>"

enable_domain_management = false

allowed_peer_coordinator_principals = [/* example: "123456789109" */]

# AWS Condition Key for Coordinator A Key Generation Enclaves. Types defined in:
# https://docs.aws.amazon.com/kms/latest/developerguide/policy-conditions.html#conditions-nitro-enclaves
# For multiple keys, PCR0 or ImageSha384 should be sufficient. These correspond to the enclave image hash
#
# Example. Uncomment and update as needed.
# keygeneration_attestation_condition_keys = {
#   "kms:RecipientAttestation:PCR0" : [
#     "85fd218fad694c4d0a722133663b181a7fcc7e5a6c88322f06a56525c91a39837685707f46f4e649cae8ff2a01dd00df",
#     "180db5b63bf34dfc58920d334f2822c63541394dd9772f94ac6e82a48d993889c4f172a7b15b7ef7945b91d4a8856229",
#     "d42ef25aa0a3fdaf1a60067c89485419b6cc66ff1c671b3b06979ced3c3c87e2b9555e468ea47c9c5da9baabcd6a6820"
#   ]
# }
