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

terraform {
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}

provider "aws" {
  region = var.aws_region
}

# auth_lambda_handler_path defaults to the zip file built via the genrule()
# in //python/privacybudget/aws/pbs_auth_handler/BUILD
# When used outside of developing within ps-coordinator-all repo, proper path must be
# explicitly given for var.auth_lambda_handler_path. In the default distribution tarball we generate,
# the path is "dist/pbs_auth_handler_lambda.zip".
locals {
  auth_lambda_handler_path = var.auth_lambda_handler_path != "" ? var.auth_lambda_handler_path : "${path.module}/../../../../../bazel-bin/python/privacybudget/aws/pbs_auth_handler/pbs_auth_handler_lambda.zip"
  use_sns_to_sqs           = var.alarms_enabled && var.sns_topic_arn != "" && var.sqs_queue_arn != ""
  account_id               = data.aws_caller_identity.current.account_id
}

data "aws_caller_identity" "current" {}

# Hosted zone must exist with CAA records for domain and wildcard subdomain
# See /applications/domainrecords for an example
data "aws_route53_zone" "hosted_zone" {
  count        = var.enable_domain_management ? 1 : 0
  name         = var.parent_domain_name
  private_zone = false
}

module "auth_db" {
  source = "../../modules/distributedpbs_auth_db"

  environment_prefix                 = var.environment
  auth_table_read_initial_capacity   = var.auth_table_read_initial_capacity
  auth_table_read_max_capacity       = var.auth_table_read_max_capacity
  auth_table_read_scale_utilization  = var.auth_table_read_scale_utilization
  auth_table_write_initial_capacity  = var.auth_table_write_initial_capacity
  auth_table_write_max_capacity      = var.auth_table_write_max_capacity
  auth_table_write_scale_utilization = var.auth_table_write_scale_utilization

  auth_table_enable_point_in_time_recovery = var.auth_table_enable_point_in_time_recovery
}

module "auth_service" {
  source = "../../modules/distributedpbs_auth_service"

  environment_prefix              = var.environment
  auth_lambda_handler_path        = local.auth_lambda_handler_path
  enable_domain_management        = var.enable_domain_management
  parent_domain_name              = var.parent_domain_name
  service_subdomain               = var.service_subdomain
  domain_hosted_zone_id           = var.enable_domain_management ? data.aws_route53_zone.hosted_zone[0].zone_id : null
  pbs_authorization_v2_table_arn  = module.auth_db.authorization_dynamo_db_table_v2_arn
  pbs_authorization_v2_table_name = module.auth_db.authorization_dynamo_db_table_v2_name
  logging_retention_days          = var.cloudwatch_logging_retention_days
}

module "beanstalk_environment" {
  source = "../../modules/distributedpbs_beanstalk_environment"

  environment_prefix                = var.environment
  enable_domain_management          = var.enable_domain_management
  parent_domain_name                = var.parent_domain_name
  service_subdomain                 = var.service_subdomain
  domain_hosted_zone_id             = var.enable_domain_management ? data.aws_route53_zone.hosted_zone[0].zone_id : null
  enable_pbs_domain_record_acme     = var.enable_domain_management ? var.enable_pbs_domain_record_acme : false
  pbs_domain_record_acme            = (var.enable_domain_management && var.enable_pbs_domain_record_acme) ? var.pbs_domain_record_acme : null
  pbs_alternate_domain_record_cname = var.enable_domain_management ? var.pbs_alternate_domain_record_cname : null
}

# Topic is used for alarms such that messages are not sensitive data.
#tfsec:ignore:aws-sns-enable-topic-encryption
resource "aws_sns_topic" "pbs" {
  name  = "${var.environment}_${var.aws_region}_pbs_sns_topic"
  count = var.alarms_enabled ? 1 : 0
  tags = {
    environment = var.environment
  }
}

resource "aws_sns_topic_subscription" "pbs" {
  count     = var.alarms_enabled ? 1 : 0
  topic_arn = local.use_sns_to_sqs ? var.sns_topic_arn : aws_sns_topic.pbs[0].arn
  protocol  = local.use_sns_to_sqs ? "sqs" : "email"
  endpoint  = local.use_sns_to_sqs ? var.sqs_queue_arn : var.alarm_notification_email
}
