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

resource "google_monitoring_alert_policy" "error_count_5xx" {
  display_name = "${var.service_prefix} Load Balancer 5xx Errors"
  combiner     = "OR"
  conditions {
    display_name = "5xx Errors"
    condition_threshold {
      filter          = "metric.type=\"loadbalancing.googleapis.com/https/request_count\" AND resource.type=\"https_lb_rule\" AND resource.label.url_map_name=\"${var.load_balancer_name}\" AND metric.label.response_code_class=\"500\""
      duration        = "${var.duration_sec}s"
      comparison      = "COMPARISON_GT"
      threshold_value = var.error_5xx_threshold
      trigger {
        count = 1
      }
      aggregations {
        alignment_period   = "${var.eval_period_sec}s"
        per_series_aligner = "ALIGN_RATE"
      }
    }
  }
  notification_channels = [var.notification_channel_id]

  user_labels = {
    environment = var.environment
  }
}

resource "google_monitoring_alert_policy" "total_latency" {
  display_name = "${var.service_prefix} Load Balancer Total Request Latency"
  combiner     = "OR"
  conditions {
    display_name = "Total Request Latency"
    condition_threshold {
      filter          = "metric.type=\"loadbalancing.googleapis.com/https/total_latencies\" AND resource.type=\"https_lb_rule\" AND resource.label.url_map_name=\"${var.load_balancer_name}\""
      duration        = "${var.duration_sec}s"
      comparison      = "COMPARISON_GT"
      threshold_value = var.max_latency_ms
      trigger {
        count = 1
      }
      aggregations {
        alignment_period   = "${var.eval_period_sec}s"
        per_series_aligner = "ALIGN_PERCENTILE_99"
      }
    }
  }
  notification_channels = [var.notification_channel_id]

  user_labels = {
    environment = var.environment
  }
}
