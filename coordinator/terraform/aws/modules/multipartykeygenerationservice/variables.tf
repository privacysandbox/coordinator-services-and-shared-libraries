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

variable "service" {
  description = "Service name for split key rotation."
  type        = string
  default     = "split-key-rotation"
}

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "split_key_rotation_template_name" {
  type        = string
  default     = "split-key-rotation-template"
  description = "The name of split key aws launch template."
}

variable "ec2_iam_role_name" {
  description = "IAM role of the ec2 instance running the split key rotation."
  type        = string
  default     = "SplitKeyEc2"
}

variable "instance_type" {
  type    = string
  default = "m5.2xlarge"
}

variable "ami_name" {
  type        = string
  default     = "sample-enclaves-linux-aws"
  description = "AMI should contain an enclave image file (EIF)"
}

variable "ami_owners" {
  type        = list(string)
  description = "AWS accounts to check for AMIs."
}

variable "keystorage_api_base_url" {
  description = "Base Url of Key Storage Api, including api stage and version but no trailing slash."
  type        = string
}

variable "keyjobqueue_url" {
  description = "Url of Key Generation Queue."
  type        = string
}

variable "keyjobqueue_arn" {
  description = "ARN of Key Generation Queue."
  type        = string
}

variable "keydb_table_name" {
  description = "DynamoDB table name for Key Database"
  type        = string
}

variable "keydb_region" {
  description = "DynamoDB region for Key Database"
  type        = string
}

variable "keydb_arn" {
  description = "DynamoDB ARN for Key Database"
  type        = string
}

variable "asg_name" {
  type        = string
  default     = "split-key-rotation"
  description = "The name of auto scaling group."
}

variable "initial_capacity_ec2_instances" {
  description = "Initial capacity for ec2 instances."
  type        = number
  default     = 1 //TODO: Change to 0 when auto scaling based on SQS message queue implemented
}

variable "max_ec2_instances" {
  description = "Upper bound for autoscaling for ec2 instances."
  type        = number
  default     = 1
}

variable "min_ec2_instances" {
  description = "Lower bound for autoscaling for ec2 instances."
  type        = number
  default     = 1 //TODO: Change to 0 when auto scaling based on SQS message queue implemented
}

variable "vpc_cidr" {
  description = "VPC CIDR range for split key VPC."
  type        = string
  default     = "172.31.0.0/16"
}

variable "az_map" {
  description = "Availability zone to number mapping for dynamic subnet rotation."
  default = {
    a = 1
    b = 2
    c = 3
    d = 4
    e = 5
    f = 6
  }
}

variable "coordinator_b_assume_role_arn" {
  description = <<-EOT
     Role to assume when interacting with Coordinator B services. If empty, no
     parameter is created and the instance's credentials are used.
  EOT
  type        = string
}

variable "enable_public_key_signature" {
  description = "Generate a public key signature key for this coordinator and include signatures."
  type        = bool
}

variable "key_id_type" {
  description = "Key ID Type"
  type        = string
}

################################################################################
# VPC Variables
################################################################################

variable "enable_vpc" {
  description = "Enable the creation of customer-owned VPCs providing networking security."
  type        = bool
}

variable "secure_vpc_subnet_ids" {
  description = "IDs of a secure private subnet used by EC2 instances."
  type        = list(string)
}

variable "secure_vpc_sg_ids" {
  description = "IDs of the security groups used by EC2."
  type        = list(string)
}

variable "dynamodb_vpc_endpoint_id" {
  description = "ID of the VPC endpoint to access DynamoDb."
  type        = string
}
