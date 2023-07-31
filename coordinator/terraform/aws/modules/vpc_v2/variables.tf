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

################################################################################
# Global Variables
################################################################################

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "enable_dynamodb_replica" {
  description = "Enable the creation of dynamodb replica."
  type        = bool
}

################################################################################
# VPC Variables
################################################################################

variable "vpc_cidr" {
  description = "VPC CIDR range for coordinator VPC."
  type        = string
}

variable "create_public_subnet" {
  description = "Enable outbound access to the public internet by setting a public subnet, NAT gateway, Internet Gateway and other necessary resources."
  type        = bool
}

variable "force_enable" {
  description = "Force enable VPC regardless of whether DynamoDb replica is available."
  type        = bool
}

variable "availability_zone_replicas" {
  description = "Number of subnet groups to create over additional availability zones in the region (up to 4 for /16, if supported by AWS region)."
  type        = number
}

variable "vpc_default_tags" {
  description = "Tags to add to network infrastructure provisioned by this module."
  type        = map(string)
}

variable "dynamodb_arn" {
  description = "ARN of the existing DynamoDb table name used by the DynamoDb VPC endpoint."
  type        = string
}

variable "lambda_execution_role_ids" {
  description = "IDs of the lambda execution roles needed for VPC access."
  type        = list(string)
}

################################################################################
# VPC Endpoint Variables
################################################################################

variable "enable_dynamodb_vpc_endpoint" {
  description = "Enable the creation of an VPC endpoint for Dynamo."
  type        = bool
}

# Note: principal_arns refer to the principal that made
# the request with the ARN specified in the policy. For IAM roles, it refers to
# the ARN of the role, not the ARN of the user that assumed the role. For example,
# Lambda or EC2 principal arns are specified as arn:aws:iam::<123456789012>:role/<role-name>.
# See AWS doc: https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_identifiers.html
variable "dynamodb_allowed_principal_arns" {
  description = "ARNs of allowed principals to access DynamoDb VPCe."
  type        = list(string)
}

variable "kms_allowed_principal_arns" {
  description = "ARNs of allowed principals to access KMS VPCe."
  type        = list(string)
}

variable "enable_kms_vpc_endpoint" {
  description = "Enable the creation of an VPC endpoint for KMS."
  type        = string
}

variable "kms_keys_arns" {
  description = "ARNs of customer-owned KMS encryption keys to access via KMS VPCe."
  type        = list(string)
}

################################################################################
# VPC Flow Log Variables
################################################################################

variable "enable_flow_logs" {
  description = "Whether to enable VPC flow logs that end up in a cloudwatch log group."
  type        = bool
}

variable "flow_logs_traffic_type" {
  description = "The traffic type to log. Either ACCEPT, REJECT or ALL."
  type        = string

  validation {
    condition     = contains(["ACCEPT", "REJECT", "ALL"], var.flow_logs_traffic_type)
    error_message = "The flow traffic type must be either ACCEPT, REJECT or ALL."
  }
}

variable "flow_logs_retention_in_days" {
  description = "The number of days to keep the flow logs in CloudWatch."
  type        = number
}
