# Copyright 2025 Google LLC
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

module "allowedoperatorgroup" {
  source = "../../../applications/allowedoperatorgroup"

  organization_domain = "<organization domain>"
  group_name_prefix   = "<prefix of the created group name>"

  # Note: the list items of owners and members should be mutually exclusive.
  owners  = ["<list of owners>"]
  members = ["<list of members>"]
}
