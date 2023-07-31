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

/// Registers component code as 0x0017 for TCP Traffic Forwarder
REGISTER_COMPONENT_CODE(SC_TCP_TRAFFIC_FORWARDER, 0x0017)

DEFINE_ERROR_CODE(SC_TCP_TRAFFIC_FORWARDER_ALREADY_RUNNING,
                  SC_TCP_TRAFFIC_FORWARDER, 0x0001,
                  "The TCP forwarder is already running.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_TCP_TRAFFIC_FORWARDER_CANNOT_START_DUE_TO_FORK_ERROR,
                  SC_TCP_TRAFFIC_FORWARDER, 0x0002,
                  "The TCP forwarder cannot be started due to forking error.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_TCP_TRAFFIC_FORWARDER_CANNOT_KILL_SOCAT_PROCESS,
                  SC_TCP_TRAFFIC_FORWARDER, 0x0003,
                  "The TCP forwarder could not kill the socat process.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors
