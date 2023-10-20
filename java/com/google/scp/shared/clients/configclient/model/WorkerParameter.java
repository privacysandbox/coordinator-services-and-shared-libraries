/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.shared.clients.configclient.model;

/** Parameters needed for the worker code. */
public enum WorkerParameter {
  // Common
  MAX_JOB_NUM_ATTEMPTS,
  MAX_JOB_PROCESSING_TIME_SECONDS,
  NOTIFICATIONS_TOPIC_ID,
  // AWS
  JOB_QUEUE,
  JOB_METADATA_DB,
  SCALE_IN_HOOK,
  COORDINATOR_A_ROLE,
  COORDINATOR_B_ROLE,
  COORDINATOR_KMS_ARN,
  ASG_INSTANCES_TABLE_NAME,
  WORKER_AUTOSCALING_GROUP,
  MAX_LIFECYCLE_ACTION_TIMEOUT_EXTENSION,
  LIFECYCLE_ACTION_HEARTBEAT_TIMEOUT,
  LIFECYCLE_ACTION_HEARTBEAT_ENABLED,
  LIFECYCLE_ACTION_HEARTBEAT_FREQUENCY,
  // GCP
  JOB_PUBSUB_TOPIC_ID,
  JOB_PUBSUB_SUBSCRIPTION_ID,
  JOB_SPANNER_INSTANCE_ID,
  JOB_SPANNER_DB_NAME,
  WORKER_MANAGED_INSTANCE_GROUP_NAME,
  GRPC_COLLECTOR_ENDPOINT,
}
