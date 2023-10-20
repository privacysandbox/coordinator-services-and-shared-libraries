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
# TODO(b/243975243): This section should be removed and use the default VPC provided by AWS
# VPC, Subnets, Security Groups
################################################################################

resource "aws_vpc" "split_key_vpc" {
  count = var.enable_vpc ? 0 : 1

  cidr_block           = var.vpc_cidr
  enable_dns_hostnames = true
  tags = {
    Name        = "${var.service}-${var.environment}"
    service     = var.service
    environment = var.environment
  }
}

data "aws_availability_zones" "split_key_az" {
  state = "available"
}

data "aws_availability_zone" "split_key_az_info" {
  for_each = toset(data.aws_availability_zones.split_key_az.names)

  name = each.key
}

resource "aws_subnet" "split_key_subnet" {
  for_each = var.enable_vpc ? {} : {
    for az, az_info in data.aws_availability_zone.split_key_az_info : az => az_info.name_suffix
    if contains(keys(var.az_map), az_info.name_suffix)
  }

  vpc_id                  = aws_vpc.split_key_vpc[0].id
  cidr_block              = cidrsubnet(aws_vpc.split_key_vpc[0].cidr_block, 8, var.az_map[each.value])
  availability_zone       = each.key
  map_public_ip_on_launch = true
  tags = {
    Name        = "${var.service}-${var.environment}"
    service     = var.service
    environment = var.environment
  }
}

resource "aws_internet_gateway" "split_key_igw" {
  count = var.enable_vpc ? 0 : 1

  vpc_id = aws_vpc.split_key_vpc[0].id
  tags = {
    Name        = "${var.service}-${var.environment}"
    service     = var.service
    environment = var.environment
  }
}

resource "aws_route_table" "split_key_route_table" {
  count = var.enable_vpc ? 0 : 1

  vpc_id = aws_vpc.split_key_vpc[0].id

  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.split_key_igw[0].id
  }
  tags = {
    Name        = "${var.service}-${var.environment}"
    service     = var.service
    environment = var.environment
  }
}

resource "aws_route_table_association" "split_key_route_table_assoc" {
  for_each = var.enable_vpc ? {} : aws_subnet.split_key_subnet

  route_table_id = aws_route_table.split_key_route_table[0].id
  subnet_id      = each.value.id
}

resource "aws_security_group" "split_key_sg" {
  count = var.enable_vpc ? 0 : 1

  name   = "split-key-sg-${var.environment}"
  vpc_id = aws_vpc.split_key_vpc[0].id
  tags = {
    Name        = "${var.service}-${var.environment}"
    service     = var.service
    environment = var.environment
  }
}

resource "aws_security_group_rule" "split_key_sg_egress" {
  count = var.enable_vpc ? 0 : 1

  type        = "egress"
  description = "split_key_sg_egress"
  from_port   = 0
  to_port     = 0
  protocol    = "-1"
  # Communicate with SCP peers and AWS services by domains.
  #tfsec:ignore:aws-ec2-no-public-egress-sgr tfsec:ignore:aws-vpc-no-public-egress-sgr
  cidr_blocks       = ["0.0.0.0/0"]
  security_group_id = aws_security_group.split_key_sg[0].id
}
