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

namespace google::scp::cpio {
// Optional. If not set, use the default value 2. The number of
// CompletionQueues for the server.
// https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_builder.html#aff66bd93cba7d4240a64550fe1fca88d
static constexpr char kJobClientCompletionQueueCount[] =
    "cmrt_sdk_job_client_completion_queue_count";
// Optional. If not set, use the default value 2. Minimum number of polling
// threads for the server.
// https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_builder.html#aff66bd93cba7d4240a64550fe1fca88d
static constexpr char kJobClientMinPollers[] =
    "cmrt_sdk_job_client_min_pollers";
// Optional. If not set, use the default value 5. Maximum number of polling
// threads for the server.
// https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_builder.html#aff66bd93cba7d4240a64550fe1fca88d
static constexpr char kJobClientMaxPollers[] =
    "cmrt_sdk_job_client_max_pollers";
// Required. The name of the job queue.
static constexpr char kJobClientJobQueueName[] =
    "cmrt_sdk_job_client_job_queue_name";
// Required. The name of the job table to use.
static constexpr char kJobClientJobTableName[] =
    "cmrt_sdk_job_client_job_table_name";
// Required for GCP. The name of the Spanner Instance to use.
static constexpr char kGcpJobClientSpannerInstanceName[] =
    "cmrt_sdk_gcp_job_client_spanner_instance_name";
// Required for GCP. The name of the Spanner Database to use.
static constexpr char kGcpJobClientSpannerDatabaseName[] =
    "cmrt_sdk_gcp_job_client_spanner_database_name";
// Optional. If not set, use the default value 2.
static constexpr char kJobClientCpuThreadCount[] =
    "cmrt_sdk_job_client_cpu_thread_count";
// Optional. If not set, use the default value 100000.
static constexpr char kJobClientCpuThreadPoolQueueCap[] =
    "cmrt_sdk_job_client_cpu_thread_pool_queue_cap";
// Optional. If not set, use the default value 2.
static constexpr char kJobClientIoThreadCount[] =
    "cmrt_sdk_job_client_io_thread_count";
// Optional. If not set, use the default value 100000.
static constexpr char kJobClientIoThreadPoolQueueCap[] =
    "cmrt_sdk_job_client_io_thread_pool_queue_cap";
}  // namespace google::scp::cpio
