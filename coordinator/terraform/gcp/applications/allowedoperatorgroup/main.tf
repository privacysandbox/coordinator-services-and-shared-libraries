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

module "allowed_user_group" {
  source = "../../modules/allowedusergroup"

  project_id          = var.project_id
  organization_domain = var.organization_domain

  group_name = "${var.group_name_prefix}-allowedopusers-external"
  owners     = var.owners
  managers   = var.managers
  members    = var.members

  initial_group_config = var.initial_group_config
  customer_id          = var.customer_id
}
