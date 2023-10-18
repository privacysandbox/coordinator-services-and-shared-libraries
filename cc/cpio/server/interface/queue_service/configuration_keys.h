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
static constexpr char kQueueClientCompletionQueueCount[] =
    "cmrt_sdk_queue_client_completion_queue_count";
// Optional. If not set, use the default value 2. Minimum number of polling
// threads for the server.
// https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_builder.html#aff66bd93cba7d4240a64550fe1fca88d
static constexpr char kQueueClientMinPollers[] =
    "cmrt_sdk_queue_client_min_pollers";
// Optional. If not set, use the default value 5. Maximum number of polling
// threads for the server.
// https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_builder.html#aff66bd93cba7d4240a64550fe1fca88d
static constexpr char kQueueClientMaxPollers[] =
    "cmrt_sdk_queue_client_max_pollers";
// Required. The name of the queue.
static constexpr char kQueueClientQueueName[] =
    "cmrt_sdk_queue_client_queue_name";
// Optional. If not set, use the default value 2.
static constexpr char kQueueClientCpuThreadCount[] =
    "cmrt_sdk_queue_client_cpu_thread_count";
// Optional. If not set, use the default value 100000.
static constexpr char kQueueClientCpuThreadPoolQueueCap[] =
    "cmrt_sdk_queue_client_cpu_thread_pool_queue_cap";
// Optional. If not set, use the default value 2.
static constexpr char kQueueClientIoThreadCount[] =
    "cmrt_sdk_queue_client_io_thread_count";
// Optional. If not set, use the default value 100000.
static constexpr char kQueueClientIoThreadPoolQueueCap[] =
    "cmrt_sdk_queue_client_io_thread_pool_queue_cap";
}  // namespace google::scp::cpio
