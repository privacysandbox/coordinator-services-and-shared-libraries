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

# Topic to publish messages
resource "google_pubsub_topic" "key_generation_pubsub_topic" {
  name = "${var.environment}-key-generation-topic"

  labels = {
    environment = var.environment
  }
}

# Topic to publish messages
resource "google_pubsub_subscription" "key_generation_pubsub_subscription" {
  name  = "${var.environment}-key-generation-subscription"
  topic = google_pubsub_topic.key_generation_pubsub_topic.name

  ack_deadline_seconds = 300

  # Set ttl to empty so the subscription is never deleted because of inactivity.
  expiration_policy {
    ttl = ""
  }

  labels = {
    environment = var.environment
  }
}

resource "google_pubsub_subscription_iam_member" "key_generation_subscriber_iam" {
  project      = var.project_id
  subscription = google_pubsub_subscription.key_generation_pubsub_subscription.name
  role         = "roles/pubsub.subscriber"
  member       = "serviceAccount:${local.key_generation_service_account_email}"
}

resource "google_pubsub_topic_iam_member" "key_generation_viewer_iam" {
  project = var.project_id
  topic   = google_pubsub_topic.key_generation_pubsub_topic.name
  role    = "roles/pubsub.viewer"
  member  = "serviceAccount:${local.key_generation_service_account_email}"
}

resource "google_cloud_scheduler_job" "key_generation_cron" {
  name        = "${var.environment}_key_generation_cron_job"
  description = "Cron job to publish message to Key Generation pubsub topic"
  schedule    = var.key_generation_cron_schedule
  time_zone   = var.key_generation_cron_time_zone

  pubsub_target {
    topic_name = format("projects/%s/topics/%s", var.project_id, google_pubsub_topic.key_generation_pubsub_topic.name)
    data       = base64encode("${var.environment}_key_generation_cron_job invoked")
  }
}
