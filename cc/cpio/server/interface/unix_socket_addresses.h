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

#pragma once

// Unix socket addresses for all SDK servers.
namespace google::scp::cpio {
static constexpr char kAutoScalingServiceAddress[] =
    "unix:///tmp/auto_scaling_service.socket";
static constexpr char kBlobStorageServiceAddress[] =
    "unix:///tmp/blob_storage_service.socket";
static constexpr char kCryptoServiceAddress[] =
    "unix:///tmp/crypto_service.socket";
static constexpr char kInstanceServiceAddress[] =
    "unix:///tmp/instance_service.socket";
static constexpr char kJobServiceAddress[] = "unix:///tmp/job_service.socket";
static constexpr char kMetricServiceAddress[] =
    "unix:///tmp/metric_service.socket";
static constexpr char kNoSqlDatabaseServiceAddress[] =
    "unix:///tmp/nosql_database_service.socket";
static constexpr char kParameterServiceAddress[] =
    "unix:///tmp/parameter_service.socket";
static constexpr char kPrivateKeyServiceAddress[] =
    "unix:///tmp/private_key_service.socket";
static constexpr char kPublicKeyServiceAddress[] =
    "unix:///tmp/public_key_service.socket";
static constexpr char kQueueServiceAddress[] =
    "unix:///tmp/queue_service.socket";
}  // namespace google::scp::cpio
