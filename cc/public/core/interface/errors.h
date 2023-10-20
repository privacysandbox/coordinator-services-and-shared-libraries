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

#ifndef SCP_CORE_INTERFACE_ERRORS_H_
#define SCP_CORE_INTERFACE_ERRORS_H_

#include <string>

namespace google::scp::core {
/**
 * @brief Gets the error message.
 *
 * @param error_code the global error code.
 * @return std::string the message about the error code.
 */
const char* GetErrorMessage(uint64_t error_code);
}  // namespace google::scp::core

#endif  // SCP_CORE_INTERFACE_ERRORS_H_
