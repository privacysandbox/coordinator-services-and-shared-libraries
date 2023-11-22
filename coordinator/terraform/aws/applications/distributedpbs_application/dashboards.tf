# Copyright 2023 Google LLC
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
module "distributed_pbs_dashboards" {
  source = "../../modules/privacybudgetdashboard"
  count  = var.alarms_enabled ? 1 : 0

  environment        = var.environment
  region             = var.aws_region
  account_id         = local.account_id
  custom_alarm_label = var.custom_alarm_label

  privacy_budget_api_gateway_id                = module.auth_service.api_gateway_id
  privacy_budget_load_balancer_id              = regex("app/.*", module.beanstalk_environment.elb_loadbalancers[0])
  privacy_budget_dashboard_time_period_seconds = var.privacy_budget_dashboard_time_period_seconds
  privacy_budget_autoscaling_group_name        = module.beanstalk_environment.autoscaling_group_name[0]
}
