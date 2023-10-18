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

// Configurations not defined in the server mode.
namespace google::scp::cpio {
// Optional. If not set, use the default value 2.
static constexpr char kSharedCpuThreadCount[] =
    "cmrt_sdk_shared_cpu_thread_count";
// Optional. If not set, use the default value 100000.
static constexpr char kSharedCpuThreadPoolQueueCap[] =
    "cmrt_sdk_shared_cpu_thread_pool_queue_cap";
// Optional. If not set, use the default value 2.
static constexpr char kSharedIoThreadCount[] =
    "cmrt_sdk_shared_io_thread_count";
// Optional. If not set, use the default value 100000.
static constexpr char kSharedIoThreadPoolQueueCap[] =
    "cmrt_sdk_shared_io_thread_pool_queue_cap";
}  // namespace google::scp::cpio
