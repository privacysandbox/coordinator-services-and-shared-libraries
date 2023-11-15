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

resource "aws_dynamodb_table_item" "coordinator_site_record" {
  table_name = var.pbs_auth_table_v2_name
  hash_key   = "adtech_identity"
  item       = <<ITEM
{
  "adtech_identity": {
    "S": "${aws_iam_role.remote_coordinator_assume_role.arn}"
  },
  "adtech_sites": {
    "SS": ["https://remote-coordinator.com"]
  }
}
ITEM
}
