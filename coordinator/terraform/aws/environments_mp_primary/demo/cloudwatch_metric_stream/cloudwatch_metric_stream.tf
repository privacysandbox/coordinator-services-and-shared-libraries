# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

locals {
  environment    = "demo"
  https_endpoint = "https://example.com"
}

module "cloudwatch_metric_stream_iam" {
  source                = "../../../applications/cloudwatch_metric_stream_iam"
  environment           = local.environment
  service_account_email = "cloudwatch@project.iam.gserviceaccount.com"
}

# Instantiate the cloudwatch_metric_stream_region for each aws provider region.
module "cloudwatch_metric_stream_us_east_1" {
  source                 = "../../../applications/cloudwatch_metric_stream_region"
  environment            = local.environment
  https_endpoint         = local.https_endpoint
  firehose_role_arn      = module.cloudwatch_metric_stream_iam.firehose_role_arn
  metric_stream_role_arn = module.cloudwatch_metric_stream_iam.metric_stream_role_arn
}

module "cloudwatch_metric_stream_us_west_2" {
  source                 = "../../../applications/cloudwatch_metric_stream_region"
  environment            = local.environment
  https_endpoint         = local.https_endpoint
  firehose_role_arn      = module.cloudwatch_metric_stream_iam.firehose_role_arn
  metric_stream_role_arn = module.cloudwatch_metric_stream_iam.metric_stream_role_arn

  providers = {
    aws = aws.us-west-2
  }
}
