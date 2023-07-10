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

locals {
  pbs_auth_domain = (var.environment_prefix != "prod" ? "auth.${var.service_subdomain}-${var.environment_prefix}.${var.parent_domain_name}" : "auth.${var.service_subdomain}.${var.parent_domain_name}")
  output_domain   = var.enable_domain_management ? "https://${local.pbs_auth_domain}" : aws_apigatewayv2_stage.api_gateway_stage.invoke_url
}

module "sslcertificate" {
  source                = "../sslcertificateprovider"
  count                 = var.enable_domain_management ? 1 : 0
  environment           = var.environment_prefix
  service_domain_name   = local.pbs_auth_domain
  domain_hosted_zone_id = var.domain_hosted_zone_id
}

resource "aws_apigatewayv2_domain_name" "pbs_auth_api_gateway_domain" {
  count       = var.enable_domain_management ? 1 : 0
  domain_name = local.pbs_auth_domain

  domain_name_configuration {
    certificate_arn = module.sslcertificate[0].acm_certificate_arn
    endpoint_type   = "REGIONAL"
    security_policy = "TLS_1_2"
  }

  # API Gateway does not keep track of certificate validation, so it might result in error if certificate is not yet created
  depends_on = [module.sslcertificate]
}

resource "aws_apigatewayv2_api_mapping" "pbs_auth_api_gateway_mapping" {
  count       = var.enable_domain_management ? 1 : 0
  api_id      = aws_apigatewayv2_api.lambda_api.id
  domain_name = aws_apigatewayv2_domain_name.pbs_auth_api_gateway_domain[0].id
  stage       = aws_apigatewayv2_stage.api_gateway_stage.id
}

resource "aws_route53_record" "pbs_auth_api_record" {
  count   = var.enable_domain_management ? 1 : 0
  name    = local.pbs_auth_domain
  type    = "A"
  zone_id = var.domain_hosted_zone_id

  alias {
    name                   = aws_apigatewayv2_domain_name.pbs_auth_api_gateway_domain[0].domain_name_configuration[0].target_domain_name
    zone_id                = aws_apigatewayv2_domain_name.pbs_auth_api_gateway_domain[0].domain_name_configuration[0].hosted_zone_id
    evaluate_target_health = false
  }
}
