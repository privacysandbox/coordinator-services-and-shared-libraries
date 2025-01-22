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

output "aws_account_to_params" {
  value = {
    for aws_account_id in local.aws_account_ids : aws_account_id => {
      workload_identity_pool_name          = google_iam_workload_identity_pool.federated_workload_identity_pool[aws_account_id].name
      workload_identity_pool_provider_name = google_iam_workload_identity_pool_provider.federated_workload_identity_pool[aws_account_id].name
      wip_federated_service_account        = google_service_account.wip_federated[aws_account_id].email
    }
  }
}
