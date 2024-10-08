// Copyright 2024 Google LLC
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

#include "cc/core/interface/errors.h"
#include "cc/public/core/interface/execution_result.h"

namespace google::scp::core {

REGISTER_COMPONENT_CODE(SC_PBS_TELEMETRY, 0x0234)

DEFINE_ERROR_CODE(SC_TELEMETRY_COULD_NOT_PARSE_URL, SC_PBS_TELEMETRY, 0x0001,
                  "Exporter could not parse the url from the options",
                  errors::HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_TELEMETRY_EXPORTER_SHUTDOWN, SC_PBS_TELEMETRY, 0x0002,
                  "Exporter is shut down",
                  errors::HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_TELEMETRY_EXPORT_FAILED, SC_PBS_TELEMETRY, 0x0003,
                  "Export failed",
                  errors::HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_TELEMETRY_GRPC_CHANNEL_CREATION_FAILED, SC_PBS_TELEMETRY,
                  0x0005, "Grpc channel creation failed",
                  errors::HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_TELEMETRY_METER_PROVIDER_NOT_INITIALIZED, SC_PBS_TELEMETRY,
                  0x0006, "Meter Provider is not initialized",
                  errors::HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core
