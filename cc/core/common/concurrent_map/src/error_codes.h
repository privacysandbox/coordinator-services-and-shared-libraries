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

#include "cc/core/interface/errors.h"

namespace privacy_sandbox::pbs_common {

/// Registers component code as 0x0003 for concurrent map.
REGISTER_COMPONENT_CODE(SC_CONCURRENT_MAP, 0x0004)

DEFINE_ERROR_CODE(SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS, SC_CONCURRENT_MAP,
                  0x0001, "The entry already exists in the concurrent map.",
                  HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST, SC_CONCURRENT_MAP,
                  0x0002, "The entry does not exist.",
                  HttpStatusCode::NOT_FOUND)

}  // namespace privacy_sandbox::pbs_common
