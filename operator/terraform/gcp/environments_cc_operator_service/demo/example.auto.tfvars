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

environment = "cc-operator-demo-env"
project_id  = "demo-project"
region      = "us-central1"
region_zone = "us-central1-c"

# Multi region location
# https://cloud.google.com/storage/docs/locations
operator_package_bucket_location = "US"

spanner_instance_config  = "regional-us-central1"
spanner_processing_units = 100 # Refer to https://cloud.google.com/spanner/docs/compute-capacity

worker_image = "<location>/<project>/<repository>/<image>:<tag or digest>"

allowed_operator_service_account = "<CoordinatorAAllowedOperatorServiceAccountEmail>,<CoordinatorBAllowedOperatorServiceAccountEmail>"

# If want to enable attestation inside TEE, onboard the pre-created SA to grant permissions.
user_provided_worker_sa_email = "<PrecreatedWorkerServiceAccountEmail>"
