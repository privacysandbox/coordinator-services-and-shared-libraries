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

locals {
  private_key_domain = (var.environment != "prod" ? "${var.service_subdomain}-${var.environment}.${var.parent_domain_name}" : "${var.service_subdomain}.${var.parent_domain_name}")
}

module "sslcertificate_private_key_service" {
  source = "../sslcertificateprovider"

  environment           = var.environment
  service_domain_name   = local.private_key_domain
  domain_hosted_zone_id = var.domain_hosted_zone_id
}

resource "aws_apigatewayv2_domain_name" "get_private_key_api_gateway_domain" {
  domain_name = local.private_key_domain

  domain_name_configuration {
    certificate_arn = module.sslcertificate_private_key_service.acm_certificate_arn
    endpoint_type   = "REGIONAL"
    security_policy = "TLS_1_2"
  }

  # API Gateway does not keep track of certificate validation, so it might result in error if certificate is not yet created
  depends_on = [module.sslcertificate_private_key_service]
}

resource "aws_apigatewayv2_api_mapping" "get_private_key_api_gateway_mapping" {
  api_id      = var.get_private_key_api_gateway_id
  domain_name = aws_apigatewayv2_domain_name.get_private_key_api_gateway_domain.id
  stage       = var.get_private_key_api_gateway_stage_id
}

resource "aws_route53_record" "get_private_key_api_gateway_record" {
  name    = local.private_key_domain
  type    = "A"
  zone_id = var.domain_hosted_zone_id

  alias {
    name                   = aws_apigatewayv2_domain_name.get_private_key_api_gateway_domain.domain_name_configuration[0].target_domain_name
    zone_id                = aws_apigatewayv2_domain_name.get_private_key_api_gateway_domain.domain_name_configuration[0].hosted_zone_id
    evaluate_target_health = false
  }
}
