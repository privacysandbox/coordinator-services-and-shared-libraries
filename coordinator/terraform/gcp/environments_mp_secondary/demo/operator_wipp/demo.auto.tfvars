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

key_encryption_key_id = "<output from mpkhs_secondary>"

allowed_wip_iam_principals = ["group:<output from allowedoperatorgroup in secondary coordinator>"]

assertion_tee_swname = "CONFIDENTIAL_SPACE"

assertion_tee_container_image_hash_list = ["<list of hash values for images>"]
