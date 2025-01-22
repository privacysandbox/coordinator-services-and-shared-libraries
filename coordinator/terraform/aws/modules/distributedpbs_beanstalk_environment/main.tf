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

locals {
  privacy_budget_domain = (var.environment_prefix != "prod" ? "${var.service_subdomain}-${var.environment_prefix}.${var.parent_domain_name}" : "${var.service_subdomain}.${var.parent_domain_name}")
  output_domain         = var.enable_domain_management ? "https://${local.privacy_budget_domain}" : null
}

resource "aws_route53_record" "pbs_domain_record" {
  count   = var.pbs_alternate_domain_record_cname != null ? 1 : 0
  zone_id = var.domain_hosted_zone_id
  name    = local.privacy_budget_domain
  type    = "CNAME"

  records = [var.pbs_alternate_domain_record_cname]
  ttl     = 60
}

resource "aws_route53_record" "pbs_domain_record_acme" {
  count   = (var.enable_domain_management && var.enable_pbs_domain_record_acme) ? 1 : 0
  zone_id = var.domain_hosted_zone_id
  name    = "_acme-challenge.${local.privacy_budget_domain}"
  type    = "CNAME"
  ttl     = 60
  records = [var.pbs_domain_record_acme]
}
