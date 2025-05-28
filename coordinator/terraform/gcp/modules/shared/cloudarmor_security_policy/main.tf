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

resource "google_compute_security_policy" "security_policy" {
  project     = var.project_id
  name        = var.security_policy_name
  description = var.security_policy_description
  type        = "CLOUD_ARMOR"

  adaptive_protection_config {
    layer_7_ddos_defense_config {
      enable = var.use_adaptive_protection

      dynamic "threshold_configs" {
        for_each = var.ddos_thresholds != null ? [1] : []
        content {
          name                               = var.ddos_thresholds.name
          detection_load_threshold           = var.ddos_thresholds.detection_load_threshold
          detection_absolute_qps             = var.ddos_thresholds.detection_absolute_qps
          detection_relative_to_baseline_qps = var.ddos_thresholds.detection_relative_to_baseline_qps
        }
      }
    }
  }

  rule {
    description = "Default allow all rule"
    action      = "allow"
    priority    = "2147483647"

    match {
      versioned_expr = "SRC_IPS_V1"

      config {
        src_ip_ranges = ["*"]
      }
    }
  }

  dynamic "rule" {
    for_each = var.security_policy_rules
    content {
      description = rule.value.description
      action      = rule.value.action
      priority    = rule.value.priority
      preview     = rule.value.preview

      match {
        # Basic rules handle IP addresses/ranges only and require both
        # versioned_expr and config be defined.
        versioned_expr = rule.value.match.versioned_expr

        dynamic "config" {
          for_each = rule.value.match.expr == null ? [1] : []
          content {
            src_ip_ranges = rule.value.match.config.src_ip_ranges
          }
        }

        # Advanced rules handle CEL expressions and require expr to be defined.
        dynamic "expr" {
          for_each = rule.value.match.expr != null ? [1] : []
          content {
            expression = rule.value.match.expr.expression
          }
        }
      }
    }
  }
}
