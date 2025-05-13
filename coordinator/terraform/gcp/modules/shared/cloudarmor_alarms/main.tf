# Copyright 2025 Google LLC
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

resource "google_monitoring_alert_policy" "dos_attack" {
  project = var.project_id
  # This ends up in a verbosity label `policyname=<display_name>`.
  display_name = "${var.service_prefix} DOS Attack"
  documentation {
    # This ends up un-expanded in an alert label `policy:description=<content>`.
    content = <<-EOT
      Cloud Armor adaptive protection has detected a possible DOS attack.

      See the [Pantheon Adaptive Protection
      Dash](https://pantheon.corp.google.com/net-security/securitypolicies/adaptiveprotection?project=$${project})
      for details.
    EOT
    # This ends up un-expanded in an alert label
    # `policy:description_format=<mime_type>`.
    mime_type = "text/markdown"
    # This ends up in the standard urls alert label, probably unexpanded,
    # hence using `${var.project_id}` to expand it in terraform instead of
    # `$${project}` which cloud only expands into its own notifications.
    links {
      display_name = "Pantheon Adaptive Protection"
      url          = "https://pantheon.corp.google.com/net-security/securitypolicies/adaptiveprotection?project=${var.project_id}"
    }
  }
  combiner = "OR"
  conditions {
    # This ends up in a verbosity label `condition:display_name=<display_name>`.
    display_name = "DOS Attack"
    condition_matched_log {
      # This ends up in a verbosity label `condition:filter=<filter>`.
      filter = <<-EOT
        resource.type="network_security_policy"
        severity="WARNING"
        resource.labels.policy_name="${var.security_policy_name}"
      EOT
      # These end up as "log-<name>" alert labels.
      label_extractors = {
        # resource.labels get turned into labels for the name  and verbosity
        # labels for the value;
        #
        # * resource_label_name[0-9]=<name>
        # * v_resource_label_value[0-9]=<value>
        #
        # However to match on the value requires a non-verbosity label, so we,
        # use this to create non-verbosity labels for these.
        policy_name = "EXTRACT(resource.labels.policy_name)"
        location    = "EXTRACT(resource.labels.location)"
        # These useful values in the jsonPayload don't appear in any other
        # labels or locations, so we create alert labels for them here.
        backend_service = "EXTRACT(jsonPayload.backendService)"
        confidence      = "EXTRACT(jsonPayload.confidence)"
        attack_size     = "EXTRACT(jsonPayload.attackSize)"
        rule_status     = "EXTRACT(jsonPayload.ruleStatus)"
        autodeployed    = "EXTRACT(jsonPayload.autoDeployed)"
      }
    }
  }

  # These end up defining the cloud alertmanager workflow lifecycle settings.
  notification_channels = var.notifications_enabled ? [var.notification_channel_id] : []
  alert_strategy {
    notification_rate_limit {
      period = "3600s" # 1h
    }
    auto_close = "43200s" # 12h
  }

  # These become a pair of alert verbosity labels in cloud-alertmanager;;
  #
  # * policy:user_label_key[0-9]=<name>
  # * policy:user_label_value[0-9]=name
  #
  # Which cloud-tee then adds a real label for matching against in prod
  # alertmanager;
  #
  # * user_label:<name> = <value>
  #
  user_labels = {
    environment = var.environment
    alertname   = "dosattack"
    severity    = "warning"
  }

  # This becomes a verbosity label policy:severity=<severity>. There is also a
  # user label "severty" we set to make a real label we can match against.
  severity = "WARNING"
}
