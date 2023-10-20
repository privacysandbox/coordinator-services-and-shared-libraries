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

environment = "<environment>"
aws_region  = "us-east-1"
# Alarm names are created with the following format:
# $Criticality$Alertname$CustomAlarmLabel
custom_alarm_label                                 = "<CustomAlarmLabel>"
coordinator_a_privacy_budgeting_endpoint           = "http://pbs-a.eba-xxxxxxxx.us-east-1.elasticbeanstalk.com/v1"
coordinator_a_privacy_budget_service_auth_endpoint = "https://pbs-a.us-east-1.amazonaws.com/pbs-a/auth"
coordinator_a_role_arn                             = "arn:aws:iam::000000000000:role/pbs-prober-role"
coordinator_a_region                               = "us-east-1"
coordinator_b_privacy_budgeting_endpoint           = "http://pbs-b.eba-xxxxxxxx.us-east-1.elasticbeanstalk.com/v1"
coordinator_b_privacy_budget_service_auth_endpoint = "https://pbs-b.amazonaws.com/pbs-b/auth"
coordinator_b_role_arn                             = "arn:aws:iam::000000000000:role/pbs-prober-role"
coordinator_b_region                               = "us-east-1"
reporting_origin                                   = "https://fake.com"

# Uncomment the following to enable alarm
# alarms_enabled = true
# alarms_notification_email = "john@gmail.com"
# alarms_sns_topic_arn = "<Your-ARN>"
# alarms_sqs_queue_arn = "<Your-ARN>"
