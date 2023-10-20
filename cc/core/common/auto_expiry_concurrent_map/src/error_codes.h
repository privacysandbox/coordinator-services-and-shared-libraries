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

/// Registers component code as 0x0015 for auto expiry concurrent map.
REGISTER_COMPONENT_CODE(SC_AUTO_EXPIRY_CONCURRENT_MAP, 0x0015)

DEFINE_ERROR_CODE(SC_AUTO_EXPIRY_CONCURRENT_MAP_CANNOT_SCHEDULE,
                  SC_AUTO_EXPIRY_CONCURRENT_MAP, 0x0001,
                  "Cannot schedule garbage collection.",
                  HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_AUTO_EXPIRY_CONCURRENT_MAP_INVALID_EXPIRATION,
                  SC_AUTO_EXPIRY_CONCURRENT_MAP, 0x0002,
                  "The expiration value is invalid.", HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED,
                  SC_AUTO_EXPIRY_CONCURRENT_MAP, 0x0004,
                  "The entry is being deleted.", HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_AUTO_EXPIRY_CONCURRENT_MAP_STOP_INCOMPLETE,
                  SC_AUTO_EXPIRY_CONCURRENT_MAP, 0x0005,
                  "There is pending work to be completed, cannot stop the "
                  "AutoExpiryConcurrentMap properly.",
                  HttpStatusCode::CONFLICT)

}  // namespace google::scp::core::errors
