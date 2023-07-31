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

output "vpc_id" {
  description = "ID of the newly created VPC."
  value       = aws_vpc.main.id
}

output "public_subnet_ids" {
  description = "ID(s) of the newly created public subnets."
  value       = var.create_public_subnet ? [for s in aws_subnet.public : s.id] : []
}

output "public_subnet_cidrs" {
  description = "CIDR IP ranges of the newly public subnets."
  value       = var.create_public_subnet ? [for s in aws_subnet.public : s.cidr_block] : []
}

output "private_subnet_ids" {
  description = "ID(s) of the newly created Private A subnets."
  value       = [for s in aws_subnet.private : s.id]
}

output "private_subnet_cidrs" {
  description = "CIDR IP ranges of the newly Private A subnets."
  value       = [for s in aws_subnet.private : s.cidr_block]
}

output "vpc_default_sg_id" {
  description = "ID of the default security group in the newly created VPC."
  value       = aws_security_group.vpc_default_sg.id
}

output "allow_egress_sg_id" {
  description = "ID of the allow_egress security group in the newly created VPC."
  value       = aws_security_group.vpc_egress_sg.id
}

output "allow_internal_ingress_sg_id" {
  description = "ID of the allow_internal security group in the newly created VPC."
  value       = aws_security_group.allow_internal_sg.id
}

output "dynamodb_vpc_endpoint_id" {
  description = "ID of the VPC endpoint accessing DynamoDb."
  value       = var.enable_dynamodb_vpc_endpoint ? aws_vpc_endpoint.dynamodb_endpoint[0].id : ""
}

output "kms_vpc_endpoint_id" {
  description = "ID of the VPC endpoint accessing KMS."
  value       = var.enable_kms_vpc_endpoint ? aws_vpc_endpoint.kms_endpoint[0].id : ""
}
