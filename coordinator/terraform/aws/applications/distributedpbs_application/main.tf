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
  auth_lambda_handler_path          = var.auth_lambda_handler_path != "" ? var.auth_lambda_handler_path : "${path.module}/../../../../../bazel-bin/python/privacybudget/aws/pbs_auth_handler/pbs_auth_handler_lambda.zip"
  container_repo_name               = (var.container_repo_name == "") ? "${var.environment}-google-scp-pbs-ecr-repository" : var.container_repo_name
  use_sns_to_sqs                    = var.alarms_enabled && var.sns_topic_arn != "" && var.sqs_queue_arn != ""
  remote_coordinator_aws_account_id = (var.remote_coordinator_aws_account_id == "" ? data.aws_caller_identity.current.account_id : var.remote_coordinator_aws_account_id)
  account_id                        = data.aws_caller_identity.current.account_id
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

module "vpc" {
  source = "../../modules/vpc_v2"
  count  = var.enable_vpc ? 1 : 0

  environment                = var.environment
  create_public_subnet       = true
  vpc_cidr                   = var.vpc_cidr
  availability_zone_replicas = var.availability_zone_replicas

  vpc_default_tags = {
    Name = "${var.environment}-google-scp-pbs-vpc"
  }

  # PBS does not use the default VPC endpoints or security configurations
  # provided by the VPC V2 service.
  force_enable                    = true
  enable_dynamodb_vpc_endpoint    = false
  enable_kms_vpc_endpoint         = false
  enable_dynamodb_replica         = false
  dynamodb_arn                    = ""
  kms_keys_arns                   = []
  kms_allowed_principal_arns      = []
  dynamodb_allowed_principal_arns = []
  lambda_execution_role_ids       = []

  enable_flow_logs            = var.enable_vpc_flow_logs
  flow_logs_traffic_type      = var.vcp_flow_logs_traffic_type
  flow_logs_retention_in_days = var.vpc_flow_logs_retention_in_days
}

module "auth_service" {
  source = "../../modules/distributedpbs_auth_service"

  environment_prefix              = var.environment
  auth_lambda_handler_path        = local.auth_lambda_handler_path
  auth_dynamo_db_table_name       = module.auth_db.auth_dynamo_db_table_name
  auth_dynamo_db_table_arn        = module.auth_db.auth_dynamo_db_table_arn
  enable_domain_management        = var.enable_domain_management
  parent_domain_name              = var.parent_domain_name
  service_subdomain               = var.service_subdomain
  domain_hosted_zone_id           = var.enable_domain_management ? data.aws_route53_zone.hosted_zone[0].zone_id : null
  pbs_authorization_v2_table_arn  = module.auth_db.authorization_dynamo_db_table_v2_arn
  pbs_authorization_v2_table_name = module.auth_db.authorization_dynamo_db_table_v2_name
}

module "access_policy" {
  source = "../../modules/distributedpbs_access_policy"

  remote_coordinator_aws_account_id = local.remote_coordinator_aws_account_id
  pbs_auth_api_gateway_arn          = module.auth_service.api_gateway_arn
  pbs_auth_table_name               = module.auth_db.auth_dynamo_db_table_name
  pbs_auth_table_v2_name            = module.auth_db.authorization_dynamo_db_table_v2_name
  remote_environment                = var.remote_environment

  depends_on = [
    module.auth_db,
    module.auth_service
  ]
}

module "storage" {
  source = "../../modules/distributedpbs_storage"

  environment_prefix                  = var.environment
  journal_s3_bucket_force_destroy     = var.journal_s3_bucket_force_destroy
  budget_table_read_capacity          = var.budget_table_read_capacity
  budget_table_write_capacity         = var.budget_table_write_capacity
  partition_lock_table_read_capacity  = var.partition_lock_table_read_capacity
  partition_lock_table_write_capacity = var.partition_lock_table_write_capacity

  budget_table_enable_point_in_time_recovery         = var.budget_table_enable_point_in_time_recovery
  partition_lock_table_enable_point_in_time_recovery = var.partition_lock_table_enable_point_in_time_recovery
}

module "beanstalk_storage" {
  source = "../../modules/distributedpbs_beanstalk_storage"

  environment_prefix                 = var.environment
  beanstalk_app_bucket_force_destroy = var.beanstalk_app_bucket_force_destroy
  pbs_aws_lb_arn                     = var.pbs_aws_lb_arn
  aws_region                         = var.aws_region
  aws_account_id                     = data.aws_caller_identity.current.account_id
}

module "beanstalk_environment" {
  source = "../../modules/distributedpbs_beanstalk_environment"

  pbs_container_port                   = "8080"
  pbs_container_health_service_port    = "9000"
  pbs_partition_lease_duration_seconds = "10"

  environment_prefix                = var.environment
  cname_prefix                      = var.cname_prefix
  aws_region                        = var.aws_region
  aws_account_id                    = data.aws_caller_identity.current.account_id
  container_repo_name               = local.container_repo_name
  remote_coordinator_aws_account_id = local.remote_coordinator_aws_account_id
  ec2_instance_type                 = var.ec2_instance_type
  beanstalk_app_version             = var.beanstalk_app_version
  root_volume_size_gb               = var.root_volume_size_gb
  autoscaling_min_size              = var.autoscaling_min_size
  autoscaling_max_size              = var.autoscaling_max_size
  pbs_s3_bucket_lb_access_logs_id   = module.beanstalk_storage.pbs_s3_bucket_lb_access_logs_id
  enable_pbs_lb_access_logs         = var.enable_pbs_lb_access_logs

  enable_domain_management = var.enable_domain_management
  parent_domain_name       = var.parent_domain_name
  service_subdomain        = var.service_subdomain
  domain_hosted_zone_id    = var.enable_domain_management ? data.aws_route53_zone.hosted_zone[0].zone_id : null

  pbs_budget_keys_dynamodb_table_name    = module.storage.budget_keys_dynamo_db_table_name
  pbs_partition_lock_dynamodb_table_name = module.storage.partition_lock_dynamo_db_table_name
  pbs_s3_bucket_name                     = module.storage.journal_s3_bucket_name
  auth_api_gateway_id                    = module.auth_service.api_gateway_id
  auth_api_gateway_base_url              = module.auth_service.api_gateway_base_url
  beanstalk_app_s3_bucket_name           = module.beanstalk_storage.beanstalk_app_s3_bucket_name

  application_environment_variables = var.application_environment_variables

  enable_vpc         = var.enable_vpc
  vpc_id             = var.enable_vpc ? module.vpc[0].vpc_id : ""
  public_subnet_ids  = var.enable_vpc ? module.vpc[0].public_subnet_ids : []
  private_subnet_ids = var.enable_vpc ? module.vpc[0].private_subnet_ids : []
  vpc_default_sg_id  = var.enable_vpc ? module.vpc[0].vpc_default_sg_id : ""

  solution_stack_name = var.beanstalk_solution_stack_name

  enable_alarms            = var.alarms_enabled
  alarm_notification_email = var.alarms_enabled ? var.alarm_notification_email : ""
  sns_topic_arn            = var.alarms_enabled ? (var.sns_topic_arn == "" ? aws_sns_topic.pbs[0].arn : var.sns_topic_arn) : ""
  sqs_queue_arn            = var.sqs_queue_arn
  depends_on = [
    module.vpc,
    module.access_policy,
    aws_sns_topic.pbs,
    module.beanstalk_storage
  ]
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
