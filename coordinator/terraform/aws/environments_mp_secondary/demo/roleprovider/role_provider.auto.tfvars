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
aws_region  = "us-west-2"

allowed_principals_map_v2 = { /* example: "123456789109" = ["foo.com"] */ }

private_key_encryptor_arn      = "<from mpkhs_secondary output>"
private_key_api_gateway_arn    = "<from mpkhs_secondary output>"
privacy_budget_api_gateway_arn = "<from distributedpbs_application output>"

privacy_budget_auth_table_name = "<from distributedpbs_application output>"

# AWS Condition Key for Nitro Enclaves. Types defined in:
# https://docs.aws.amazon.com/kms/latest/developerguide/policy-conditions.html#conditions-nitro-enclaves
# For multiple keys, PCR0 or ImageSha384 should be sufficient. These correspond to the enclave image hash
# Example. Uncomment and update as needed.

#attestation_condition_keys = {
#  "kms:RecipientAttestation:PCR0" : [
#    "dde96c25d0766825f9746651a0a3a20960d818313d89af380b25a8d9347c58eaeee8f3e35fc1a7e1cc1f672c3fc63f5e",
#    "4644f46180661f2f15ca6c7caed62bff33356659466ba93e983b7ae9e1405e51b3c20d5c606b7327d8bb5ae47835524d",
#    "4a9d4bebda73694552d1bea7bf1b5e2a3758b075ff1a1240ddbe319eb140355e52b53a3e130bcafab4bae8513a27cdf0"
#  ]
#}
