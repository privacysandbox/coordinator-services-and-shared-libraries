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

provider "aws" {
  region = var.aws_region
}

terraform {
  required_providers {
    aws = {
      source = "hashicorp/aws"
    }
  }
}


module "adtechmanagement_db" {
  source      = "../../modules/adtechmanagement_db"
  environment = var.environment
}

module "adtechmanagement_storage" {
  source      = "../../modules/adtechmanagement_storage"
  environment = var.environment
}

module "adtechmanagement_service" {
  source                                      = "../../modules/adtechmanagement_service"
  handle_onboarding_request_lambda_file_path  = var.handle_onboarding_request_lambda_file_path
  handle_offboarding_request_lambda_file_path = var.handle_offboarding_request_lambda_file_path
  handle_read_adtech_request_lambda_file_path = var.handle_read_adtech_request_lambda_file_path
  log_retention_in_days                       = var.log_retention_in_days
  environment                                 = var.environment
  aws_region                                  = var.aws_region
  pbs_auth_db_arn                             = var.pbs_auth_db_arn
  pbs_auth_db_name                            = var.pbs_auth_db_name
  adtech_mgmt_db_arn                          = module.adtechmanagement_db.arn
  adtech_mgmt_db_name                         = module.adtechmanagement_db.name
  iam_policy_arn                              = var.iam_policy_arn
  adtech_mgmt_storage_bucket_name             = module.adtechmanagement_storage.bucket
}
