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

output "lambda_version" {
  value = module.pbsprober.lambda_version
}

output "lambda_assumed_role" {
  value = module.pbsprober.lambda_assumed_role
}

output "pbsprober_lambda_principal_role_arn" {
  value = module.pbsprober.pbsprober_lambda_principal_role_arn
}

output "pbsprober_lambda_principal_role_id" {
  value = module.pbsprober.pbsprober_lambda_principal_role_id
}
