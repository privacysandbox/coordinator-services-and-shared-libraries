// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {

/// Registers component code as 0x011A for the partition lease sink.
REGISTER_COMPONENT_CODE(SC_PARTITION_LEASE_EVENT_SINK, 0x011B)

DEFINE_ERROR_CODE(SC_PARTITION_LEASE_EVENT_SINK_NOT_RUNNING,
                  SC_PARTITION_LEASE_EVENT_SINK, 0x0001,
                  "Partition lease event sink not running",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(
    SC_PARTITION_LEASE_EVENT_SINK_LOAD_TASK_RUNNING_WHILE_RELEASE,
    SC_PARTITION_LEASE_EVENT_SINK, 0x0002,
    "A partition load task is found running while trying to release the lease",
    HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_PARTITION_LEASE_EVENT_SINK_TASK_RUNNING_WHILE_LOST,
                  SC_PARTITION_LEASE_EVENT_SINK, 0x0003,
                  "A partition task is found running while the lease is lost",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(
    SC_PARTITION_LEASE_EVENT_SINK_TASK_RUNNING_WHILE_ACQUIRE,
    SC_PARTITION_LEASE_EVENT_SINK, 0x0004,
    "A partition task is found running while the lease is being acquired",
    HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_PARTITION_LEASE_EVENT_SINK_CANNOT_EMPLACE_TO_MAP,
                  SC_PARTITION_LEASE_EVENT_SINK, 0x0005,
                  "A partition task cannot be emplaced into the tasks map",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_PARTITION_LEASE_EVENT_SINK_CANNOT_INIT_METRICS,
                  SC_PARTITION_LEASE_EVENT_SINK, 0x0006,
                  "Cannot initialize/run metrics for partitions",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

}  // namespace google::scp::core::errors
