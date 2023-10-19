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

/// Registers component code as 0x010A for the PBS server.
REGISTER_COMPONENT_CODE(SC_PBS_SERVER, 0x010A)

DEFINE_ERROR_CODE(SC_PBS_SERVER_CRASHED, SC_PBS_SERVER, 0x0001,
                  "PBS server crashed.", HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_PBS_SERVER_CHILD_SIGNALLED, SC_PBS_SERVER, 0x0002,
                  "PBS server's child signalled.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors
