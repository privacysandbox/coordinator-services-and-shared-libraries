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
  required_version = "~> 1.2.3"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}

data "aws_region" "current" {}

# Get information about current availability zones.
data "aws_availability_zones" "avail" {
  state = "available"
}

locals {
  region = data.aws_region.current.name

  # At most we can use the number of avilable zones for replicas
  actual_availability_zone_replicas = min(var.availability_zone_replicas, length(data.aws_availability_zones.avail.names))

  # cidrsubnets() creates a list of consecutive subnets with the specified
  # additional bits added to the base subnet mask.
  # The module defines the new subnet masks as follows:
  # - 3 bits for Private Subnet (/19 with a /16 VPC)
  # - 4 bits for the public subnet (/20 with a /16 VPC)
  # - 5 bits for custom subnets (padding only - not provisioned by this module)
  # The subnets are built out of 4 larger netblocks to allow for AZ replication.
  subnets = [for cidr_block in cidrsubnets(var.vpc_cidr, 2, 2, 2, 2) : cidrsubnets(cidr_block, 1, 2, 3)]
}

################################################################################
# VPC, Subnets, Security Groups, and ACL
################################################################################

resource "aws_vpc" "main" {
  cidr_block = var.vpc_cidr

  # Note: Do not remove; Necessary to enable private DNS for aws_vpc_endpoint
  enable_dns_support   = true
  enable_dns_hostnames = true

  tags = var.vpc_default_tags

  lifecycle {
    precondition {
      # Both dynamodb replica and lambda vpc need to be both enabled in conjunction.
      # Otherwise, throw appropriate error message at runtime
      condition     = var.force_enable || var.enable_dynamodb_replica
      error_message = <<-EOT
        DynamoDb replica must be enabled in conjunction with Coordinator VPC to ensure
        that a DynamoDb instance exists in the same region as the Coordinator VPC.
        Cross-region access of private resources is not supported until VPC peering
        is added.
      EOT
    }
  }
}

# Private Subnet
resource "aws_subnet" "private" {
  count             = local.actual_availability_zone_replicas
  vpc_id            = aws_vpc.main.id
  availability_zone = data.aws_availability_zones.avail.names[count.index]
  cidr_block        = local.subnets[count.index][0]
  tags = merge(
    {
      subnet_type = "private"
    },
    {
      availability_zone = data.aws_availability_zones.avail.names[count.index]
    },
    var.vpc_default_tags
  )

  # If the IAM Role permissions are removed before Lambda is able to delete its
  # Hyperplane ENIs, the subnet/security groups deletions will continually fail.
  # Mark an explicit dependency so that Lambda execution role is preserved until
  # after the subnet/security group is deleted.
  depends_on = [
    aws_iam_role_policy_attachment.lambda_vpc_access_execution_role_attachment
  ]
}

# Public Subnet
resource "aws_subnet" "public" {
  count = var.create_public_subnet ? local.actual_availability_zone_replicas : 0

  vpc_id            = aws_vpc.main.id
  availability_zone = data.aws_availability_zones.avail.names[count.index]
  cidr_block        = local.subnets[count.index][1]
  tags = merge(
    {
      subnet_type = "public"
    },
    {
      availability_zone = data.aws_availability_zones.avail.names[count.index]
    },
    var.vpc_default_tags
  )
}

# Create a default security group for the VPC.
# Note that without creating additional rules, this will deny all inbound and
# outbound traffic to member hosts.
resource "aws_security_group" "vpc_default_sg" {
  vpc_id      = aws_vpc.main.id
  description = "Default security group for VPC ${aws_vpc.main.id} - Managed by Terraform"
  tags        = var.vpc_default_tags
}

resource "aws_security_group" "allow_internal_sg" {
  name        = "allow_internal"
  vpc_id      = aws_vpc.main.id
  description = "VPC allowing unrestricted internal traffic for member hosts in VPC ${aws_vpc.main.id} - Managed by Terraform"
  tags        = var.vpc_default_tags
}

resource "aws_security_group_rule" "allow_internal_sg_rule" {
  type              = "ingress"
  description       = "allow_internal_sg_rule"
  protocol          = "all"
  from_port         = 0
  to_port           = 0
  cidr_blocks       = [aws_vpc.main.cidr_block]
  security_group_id = aws_security_group.allow_internal_sg.id

  # Necessary workaround to avoid recreation errors. See
  # https://github.com/hashicorp/terraform-provider-aws/issues/12420
  lifecycle {
    create_before_destroy = true
  }
}

# NB: Necessary for clients in the VPC to establish connections to interface type
# VPC endpoints such the KMS endpoint. See stackoverflow:
# https://stackoverflow.com/a/63660501
resource "aws_security_group_rule" "coordinator_sg_ingress_https" {
  type              = "ingress"
  description       = "https access from VPC"
  from_port         = 443
  to_port           = 443
  protocol          = "tcp"
  cidr_blocks       = [aws_vpc.main.cidr_block]
  security_group_id = aws_security_group.allow_internal_sg.id
  # Necessary workaround to avoid recreation errors. See
  # https://github.com/hashicorp/terraform-provider-aws/issues/12420
  lifecycle {
    create_before_destroy = true
  }
}

# Create an additional security group used to allow egress to the Internet.
resource "aws_security_group" "vpc_egress_sg" {
  name        = "allow_egress"
  vpc_id      = aws_vpc.main.id
  description = "VPC allowing unrestricted outbound Internet egress for member hosts in VPC ${aws_vpc.main.id} - Managed by Terraform"
  tags        = var.vpc_default_tags
}

resource "aws_security_group_rule" "coordinator_sg_egress_http" {
  type      = "egress"
  from_port = 80
  to_port   = 80
  protocol  = "tcp"
  # Communicate with SCP peers and AWS services by domains.
  #tfsec:ignore:aws-ec2-no-public-egress-sgr tfsec:ignore:aws-vpc-no-public-egress-sgr
  cidr_blocks       = ["0.0.0.0/0"]
  description       = "http access"
  security_group_id = aws_security_group.vpc_egress_sg.id
  # Necessary workaround to avoid recreation errors. See
  # https://github.com/hashicorp/terraform-provider-aws/issues/12420
  lifecycle {
    create_before_destroy = true
  }
}

resource "aws_security_group_rule" "coordinator_sg_egress_https" {
  type      = "egress"
  from_port = 443
  to_port   = 443
  protocol  = "tcp"
  # Communicate with SCP peers and AWS services by domains.
  #tfsec:ignore:aws-ec2-no-public-egress-sgr tfsec:ignore:aws-vpc-no-public-egress-sgr
  cidr_blocks       = ["0.0.0.0/0"]
  description       = "https access"
  security_group_id = aws_security_group.vpc_egress_sg.id
  # Necessary workaround to avoid recreation errors. See
  # https://github.com/hashicorp/terraform-provider-aws/issues/12420
  lifecycle {
    create_before_destroy = true
  }
}

resource "aws_security_group_rule" "coordinator_sg_egress_dns" {
  type      = "egress"
  from_port = 53
  to_port   = 53
  protocol  = "tcp"
  # Communicate with DNS at unknown IPs.
  #tfsec:ignore:aws-ec2-no-public-egress-sgr tfsec:ignore:aws-vpc-no-public-egress-sgr
  cidr_blocks       = ["0.0.0.0/0"]
  description       = "dns via tcp access"
  security_group_id = aws_security_group.vpc_egress_sg.id
  # Necessary workaround to avoid recreation errors. See
  # https://github.com/hashicorp/terraform-provider-aws/issues/12420
  lifecycle {
    create_before_destroy = true
  }
}

# DNS has always been designed to use both UDP and TCP port 53 from the start
# with UDP being the default, and fall back to using TCP when it is unable to
# communicate on UDP, typically when the packet size is too large to push through
# in a single UDP packet.
resource "aws_security_group_rule" "coordinator_sg_egress_dns_udp" {
  type      = "egress"
  from_port = 53
  to_port   = 53
  protocol  = "udp"
  # Communicate with DNS at unknown IPs.
  #tfsec:ignore:aws-ec2-no-public-egress-sgr tfsec:ignore:aws-vpc-no-public-egress-sgr
  cidr_blocks       = ["0.0.0.0/0"]
  description       = "dns via udp access"
  security_group_id = aws_security_group.vpc_egress_sg.id
  lifecycle {
    create_before_destroy = true
  }
}

# AWS services like Elastic Beanstalk require NTP communication.
# https://docs.aws.amazon.com/elasticbeanstalk/latest/dg/vpc.html
resource "aws_security_group_rule" "coordinator_sg_egress_ntp_udp" {
  type      = "egress"
  from_port = 123
  to_port   = 123
  protocol  = "udp"
  # Communicate with AWS services over NTP at unknown IPs.
  #tfsec:ignore:aws-ec2-no-public-egress-sgr tfsec:ignore:aws-vpc-no-public-egress-sgr
  cidr_blocks       = ["0.0.0.0/0"]
  description       = "ntp via udp access"
  security_group_id = aws_security_group.vpc_egress_sg.id
  lifecycle {
    create_before_destroy = true
  }
}

################################################################################
# Internet and NAT Gateway
################################################################################

resource "aws_internet_gateway" "inet_gateway" {
  count = var.create_public_subnet ? 1 : 0

  vpc_id = aws_vpc.main.id
  tags   = var.vpc_default_tags
}

resource "aws_route" "inet_gateway_route" {
  count = var.create_public_subnet ? 1 : 0

  route_table_id         = aws_vpc.main.default_route_table_id
  destination_cidr_block = "0.0.0.0/0"
  gateway_id             = aws_internet_gateway.inet_gateway[0].id
}

# Create Elastic IPs for the NAT Gateway(s).
resource "aws_eip" "elastic_ip" {
  count = var.create_public_subnet ? local.actual_availability_zone_replicas : 0
  vpc   = true
}

# Create NAT gateway(s) in public subnets to allow egress from Private subnet.
# The current version of the AWS G3 TF provider does not support private NAT.
resource "aws_nat_gateway" "nat_gateway" {
  count = var.create_public_subnet ? local.actual_availability_zone_replicas : 0
  # The NAT Gateway is created in the public subnet to provide access to
  # private subnets, which route to it as a default gateway.
  subnet_id     = aws_subnet.public[count.index].id
  allocation_id = aws_eip.elastic_ip[count.index].id
  depends_on    = [aws_internet_gateway.inet_gateway]
  tags = merge(
    {
      subnet_type = "public"
    },
    {
      availability_zone = data.aws_availability_zones.avail.names[count.index]
    },
    var.vpc_default_tags
  )
}

################################################################################
# Routes
################################################################################

# Create subnet route tables for Private Subnet, to enable NAT Gateway routing.
resource "aws_route_table" "private_route_table" {
  count  = local.actual_availability_zone_replicas
  vpc_id = aws_vpc.main.id
  tags = merge(
    {
      subnet_type = "private"
    },
    {
      availability_zone = data.aws_availability_zones.avail.names[count.index]
    },
    var.vpc_default_tags
  )
}

resource "aws_route_table_association" "private_route_assoc" {
  count          = local.actual_availability_zone_replicas
  subnet_id      = aws_subnet.private[count.index].id
  route_table_id = aws_route_table.private_route_table[count.index].id
}

# Add a default route to the NAT gateway attached to each public subnet.
resource "aws_route" "nat_gateway_route" {
  count                  = var.create_public_subnet ? local.actual_availability_zone_replicas : 0
  route_table_id         = aws_route_table.private_route_table[count.index].id
  destination_cidr_block = "0.0.0.0/0"
  nat_gateway_id         = aws_nat_gateway.nat_gateway[count.index].id
}

resource "aws_vpc_endpoint_route_table_association" "private_to_keydb_route" {
  count = var.enable_dynamodb_vpc_endpoint ? local.actual_availability_zone_replicas : 0

  vpc_endpoint_id = aws_vpc_endpoint.dynamodb_endpoint[0].id
  route_table_id  = aws_route_table.private_route_table[count.index].id
}

################################################################################
# Lambda Role Policy Attachments
# Policy attachment to Lambda role to access VPC execution; necessary for the lifecycle
# a Lambda, including attaching to a custom VPC and deleting leftover ENIs.
# See https://docs.aws.amazon.com/lambda/latest/dg/configuration-vpc.html#vpc-permissions
################################################################################
resource "aws_iam_role_policy_attachment" "lambda_vpc_access_execution_role_attachment" {
  for_each = { for i, id in var.lambda_execution_role_ids : i => id }

  role       = each.value
  policy_arn = "arn:aws:iam::aws:policy/service-role/AWSLambdaVPCAccessExecutionRole"
}
