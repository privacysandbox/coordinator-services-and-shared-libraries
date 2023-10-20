/**
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

# Example values required by operator_service.tf
#
# These values should be modified for each of your environments.

environment = "cc-postsubmit"
project_id  = "admcloud-adtech1"
region      = "us-central1"
region_zone = "us-central1-c"

spanner_instance_config  = "regional-us-central1"
spanner_processing_units = 100

worker_image                         = "us-docker.pkg.dev/admcloud-scp/docker-repo-dev/worker_app_mp_gcp:cc-postsubmit"
allowed_operator_service_account     = "postsubmit-a-opallowedusr@admcloud-coordinator1.iam.gserviceaccount.com,postsubmit-b-opallowedusr@admcloud-coordinator2.iam.gserviceaccount.com"
worker_logging_enabled               = true
worker_instance_force_replace        = true
user_provided_worker_sa_email        = "cc-postsubmit-worker@admcloud-adtech1.iam.gserviceaccount.com"
spanner_database_deletion_protection = false
