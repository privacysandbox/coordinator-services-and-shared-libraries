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
  topic_id        = google_pubsub_topic.key_generation_pubsub_topic.name
  subscription_id = google_pubsub_subscription.key_generation_pubsub_subscription.name
}

resource "google_monitoring_alert_policy" "keygen_pubsub_too_many_undelivered_messages_alert" {
  count        = var.alarms_enabled ? 1 : 0
  display_name = "${var.environment} Key Generation Pub/Sub Too Many Undelivered Messages"
  combiner     = "OR"
  conditions {
    display_name = "Undelivered Messages"
    condition_threshold {
      filter          = "resource.type = \"pubsub_subscription\" AND metric.type = \"pubsub.googleapis.com/subscription/num_undelivered_messages\" AND metadata.system_labels.topic_id = \"${local.topic_id}\""
      duration        = "0s"
      comparison      = "COMPARISON_GT"
      threshold_value = var.undelivered_messages_threshold
      trigger {
        count = 1
      }
      aggregations {
        alignment_period   = "${var.key_generation_alignment_period}s"
        per_series_aligner = "ALIGN_MAX"
      }
    }
  }
  notification_channels = [var.notification_channel_id]

  user_labels = {
    environment = var.environment
  }
}

resource "google_monitoring_dashboard" "key_generation_dashboard" {
  dashboard_json = jsonencode(
    {
      "displayName" : "${var.environment} Key Generation Dashboard",
      "gridLayout" : {
        "columns" : "2",
        "widgets" : [
          {
            "title" : "Pub/Sub Message Operations",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "legendTemplate" : "Messages Finished Processing",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "resource.label.\"subscription_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_SUM"
                      },
                      "filter" : "metric.type=\"pubsub.googleapis.com/subscription/pull_ack_request_count\" resource.type=\"pubsub_subscription\" resource.label.\"subscription_id\"=\"${local.subscription_id}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s"
                      }
                    }
                  }
                },
                {
                  "legendTemplate" : "Messages Created",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "resource.label.\"topic_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_SUM"
                      },
                      "filter" : "metric.type=\"pubsub.googleapis.com/topic/send_request_count\" resource.type=\"pubsub_topic\" resource.label.\"topic_id\"=\"${local.topic_id}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s"
                      }
                    }
                  }
                },
                {
                  "legendTemplate" : "Messages Delivered",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "resource.label.\"subscription_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_SUM"
                      },
                      "filter" : "metric.type=\"pubsub.googleapis.com/subscription/sent_message_count\" resource.type=\"pubsub_subscription\" resource.label.\"subscription_id\"=\"${local.subscription_id}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s"
                      }
                    }
                  }
                }
              ],
              "timeshiftDuration" : "0s",
              "y2Axis" : {
                "label" : "y2Axis",
                "scale" : "LINEAR"
              }
            }
          },
          {
            "title" : "Pub/Sub Messages In-Flight",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "legendTemplate" : "Delivered Messages",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "resource.label.\"subscription_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_MAX"
                      },
                      "filter" : "metric.type=\"pubsub.googleapis.com/subscription/sent_message_count\" resource.type=\"pubsub_subscription\" resource.label.\"subscription_id\"=\"${local.subscription_id}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s"
                      }
                    }
                  }
                },
                {
                  "legendTemplate" : "Undelivered Messages",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "resource.label.\"subscription_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_MAX"
                      },
                      "filter" : "metric.type=\"pubsub.googleapis.com/subscription/num_undelivered_messages\" resource.type=\"pubsub_subscription\" resource.label.\"subscription_id\"=\"${local.subscription_id}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s"
                      }
                    }
                  }
                }
              ],
              "timeshiftDuration" : "0s",
              "y2Axis" : {
                "label" : "y2Axis",
                "scale" : "LINEAR"
              }
            }
          },
          {
            "title" : "KeyDb Scanned and Returned Rows",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "legendTemplate" : "Returned Rows",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "resource.label.\"instance_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_SUM"
                      },
                      "filter" : "metric.type=\"spanner.googleapis.com/query_stat/total/returned_rows_count\" resource.type=\"spanner_instance\" resource.label.\"instance_id\"=\"${var.keydb_instance_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s"
                      }
                    }
                  }
                },
                {
                  "legendTemplate" : "Scanned Rows",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "resource.label.\"instance_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_SUM"
                      },
                      "filter" : "metric.type=\"spanner.googleapis.com/query_stat/total/scanned_rows_count\" resource.type=\"spanner_instance\" resource.label.\"instance_id\"=\"${var.keydb_instance_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s"
                      }
                    }
                  }
                }
              ],
              "timeshiftDuration" : "0s",
              "y2Axis" : {
                "label" : "y2Axis",
                "scale" : "LINEAR"
              }
            }
          },
          {
            "title" : "KeyDb Read/Write API Request Latency",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "legendTemplate" : "p99 \u0024{metric.labels.method}",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "perSeriesAligner" : "ALIGN_PERCENTILE_99"
                      },
                      "filter" : "metric.type=\"spanner.googleapis.com/api/request_latencies_by_transaction_type\" resource.type=\"spanner_instance\" resource.label.\"instance_id\"=\"${var.keydb_instance_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_MEAN",
                        "groupByFields" : [
                          "metric.label.\"method\""
                        ],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      }
                    }
                  }
                },
                {
                  "legendTemplate" : "p95 \u0024{metric.labels.method}",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "perSeriesAligner" : "ALIGN_PERCENTILE_95"
                      },
                      "filter" : "metric.type=\"spanner.googleapis.com/api/request_latencies_by_transaction_type\" resource.type=\"spanner_instance\" resource.label.\"instance_id\"=\"${var.keydb_instance_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_MEAN",
                        "groupByFields" : [
                          "metric.label.\"method\""
                        ],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      }
                    }
                  }
                },
                {
                  "legendTemplate" : "p50 \u0024{metric.labels.method}",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "perSeriesAligner" : "ALIGN_PERCENTILE_50"
                      },
                      "filter" : "metric.type=\"spanner.googleapis.com/api/request_latencies_by_transaction_type\" resource.type=\"spanner_instance\" resource.label.\"instance_id\"=\"${var.keydb_instance_name}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_MEAN",
                        "groupByFields" : [
                          "metric.label.\"method\""
                        ],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      }
                    }
                  }
                }
              ],
              "timeshiftDuration" : "0s",
              "y2Axis" : {
                "label" : "y2Axis",
                "scale" : "LINEAR"
              }
            }
          },
          {
            "title" : "KeyDb API Request Error Rate",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "legendTemplate" : "API Request Errors",
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "resource.label.\"instance_id\""
                        ],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      },
                      "filter" : "metric.type=\"spanner.googleapis.com/api/request_count\" resource.type=\"spanner_instance\" resource.label.\"instance_id\"=\"${var.keydb_instance_name}\" metric.label.\"status\"!=\"OK\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s"
                      }
                    }
                  }
                }
              ],
              "timeshiftDuration" : "0s",
              "y2Axis" : {
                "label" : "y2Axis",
                "scale" : "LINEAR"
              }
            }
          },
          {
            "title" : "KeyDb API Request Error Percentage",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "legendTemplate" : "API Request Errors",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilterRatio" : {
                      "denominator" : {
                        "aggregation" : {
                          "alignmentPeriod" : "60s",
                          "crossSeriesReducer" : "REDUCE_SUM",
                          "groupByFields" : [
                            "resource.label.\"instance_id\""
                          ],
                          "perSeriesAligner" : "ALIGN_SUM"
                        },
                        "filter" : "metric.type=\"spanner.googleapis.com/api/api_request_count\" resource.type=\"spanner_instance\" resource.label.\"instance_id\"=\"${var.keydb_instance_name}\""
                      },
                      "numerator" : {
                        "aggregation" : {
                          "alignmentPeriod" : "60s",
                          "crossSeriesReducer" : "REDUCE_SUM",
                          "groupByFields" : [
                            "resource.label.\"instance_id\""
                          ],
                          "perSeriesAligner" : "ALIGN_SUM"
                        },
                        "filter" : "metric.type=\"spanner.googleapis.com/api/api_request_count\" resource.type=\"spanner_instance\" resource.label.\"instance_id\"=\"${var.keydb_instance_name}\" metric.label.\"status\"!=\"OK\""
                      }
                    }
                  }
                }
              ],
              "timeshiftDuration" : "0s",
              "y2Axis" : {
                "label" : "y2Axis",
                "scale" : "LINEAR"
              }
            }
          },
          {
            "title" : "VM CPU Utilization",
            "xyChart" : {
              "chartOptions" : {
                "mode" : "COLOR"
              },
              "dataSets" : [
                {
                  "minAlignmentPeriod" : "60s",
                  "plotType" : "LINE",
                  "targetAxis" : "Y2",
                  "timeSeriesQuery" : {
                    "timeSeriesFilter" : {
                      "aggregation" : {
                        "alignmentPeriod" : "60s",
                        "crossSeriesReducer" : "REDUCE_SUM",
                        "groupByFields" : [
                          "metric.label.\"instance_name\""
                        ],
                        "perSeriesAligner" : "ALIGN_MEAN"
                      },
                      "filter" : "metric.type=\"compute.googleapis.com/instance/cpu/utilization\" resource.type=\"gce_instance\" metadata.user_labels.\"environment\"=\"${var.environment}\"",
                      "secondaryAggregation" : {
                        "alignmentPeriod" : "60s"
                      }
                    }
                  }
                }
              ],
              "timeshiftDuration" : "0s",
              "y2Axis" : {
                "label" : "y2Axis",
                "scale" : "LINEAR"
              }
            }
          }
        ]
      }
    }
  )
}
