/**
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

terraform {
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }

  required_version = ">= 1.2.0"
}

provider "aws" {
  region  = "us-east-1"
}

resource "aws_security_group" "test_allow_all" {
  name        = "allow_all"
  description = "Allow all traffic"

  ingress {
    description = "Inbound TCP on port 22 within VPC"
    from_port   = 22
    to_port     = 22
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  egress {
    from_port        = 0
    to_port          = 0
    protocol         = "-1"
    cidr_blocks      = ["0.0.0.0/0"]
  }

  tags = {
    Name = "allow_all"
  }
}

resource "aws_network_interface" "test_network" {
  subnet_id   = var.subnet_id
  security_groups = [
    aws_security_group.test_allow_all.id
  ]
}

data "aws_iam_role" "test_iam_role" {
  name = var.iam_role
}

resource "aws_iam_instance_profile" "test_profile" {
  name = "profile"
  role = data.aws_iam_role.test_iam_role.name
}

resource "aws_instance" "test" {
  ami           = var.ami
  instance_type = "m5.xlarge"
  enclave_options {
    enabled = true
  }

  # Enforce IMDSv2.
  metadata_options {
    http_endpoint          = "enabled"
    http_tokens            = "required"
    http_put_response_hop_limit = 2
    instance_metadata_tags = "enabled"
  }

  iam_instance_profile = aws_iam_instance_profile.test_profile.name
  key_name = var.key_pair_name

  network_interface {
    network_interface_id = aws_network_interface.test_network.id
    device_index         = 0
  }

  tags = {
    Name = var.instance_name
    environment = "test"
  }
}
