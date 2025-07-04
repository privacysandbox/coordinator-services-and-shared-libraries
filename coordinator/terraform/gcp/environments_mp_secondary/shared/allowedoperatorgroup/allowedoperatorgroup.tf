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

########################
# DO NOT EDIT MANUALLY #
########################

# This file is meant to be shared across all environments (either copied or
# symlinked). In order to make the upgrade process easier, this file should not
# be modified for environment-specific customization.

module "allowedoperatorgroup" {
  source = "../../../applications/allowedoperatorgroup"

  organization_domain = var.organization_domain
  group_name_prefix   = var.group_name_prefix

  owners   = var.owners
  managers = var.managers
  members  = var.members

  initial_group_config = var.initial_group_config
  customer_id          = var.customer_id
}
