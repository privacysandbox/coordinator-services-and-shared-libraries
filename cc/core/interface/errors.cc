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

#include "errors.h"

#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {
std::map<uint64_t, std::map<uint64_t, SCPError>>& GetGlobalErrorCodes() {
  /// Defines global_error_codes to store all error codes.
  static std::map<uint64_t, std::map<uint64_t, SCPError>> global_error_codes;
  return global_error_codes;
}

std::map<uint64_t, uint64_t>& GetPublicErrorCodesMap() {
  /// Defines public_error_codes_map to store error codes and associated public
  /// error code.
  static std::map<uint64_t, uint64_t> public_error_codes_map;
  return public_error_codes_map;
}
}  // namespace google::scp::core::errors
