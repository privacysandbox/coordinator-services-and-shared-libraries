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

#include <string>

#include "public/core/interface/execution_result.h"

namespace google::scp::core::utils {
/**
 * @brief Decode \a encoded as base64 encoded string, output to \a decoded.
 *
 * @param[in] encoded input encoded string.
 * @param[out] decoded output decoded string.
 * @return ExecutionResult The execution result of the operation.
 */
ExecutionResult Base64Decode(const std::string& encoded, std::string& decoded);

/**
 * @brief Encodes values to base64.
 *
 * @param decoded The input value to be encoded.
 * @param encoded The output value.
 * @return ExecutionResult The execution result of the operation.
 */
ExecutionResult Base64Encode(const std::string& decoded, std::string& encoded);

/**
 * @brief Append [0, 2] '=' to \a encoded and return the result. This is for use
 * with services which maybe don't correctly pad their Base64 encodings.
 *
 * If encoded.length() % 4 == 1, returns encoded. This scenario should never
 * happen unless the original encoding method is incorrect.
 *
 * @param encoded  The input value to be padded.
 * @return ExecutionResultOr<string> The padded value.
 */
ExecutionResultOr<std::string> PadBase64Encoding(const std::string& encoded);

}  // namespace google::scp::core::utils
