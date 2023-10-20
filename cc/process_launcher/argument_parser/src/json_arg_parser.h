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
#include <vector>

#include <nlohmann/json.hpp>

#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::process_launcher {
struct ExecutableArgument {
  std::string executable_name;
  std::vector<std::string> command_line_args;
  bool restart = true;

  void ToExecutableVector(std::vector<char*>& cstring_vec) {
    cstring_vec.reserve(command_line_args.size() + 2);
    cstring_vec.push_back(const_cast<char*>(executable_name.c_str()));
    for (auto& s : command_line_args) {
      cstring_vec.push_back(const_cast<char*>(s.c_str()));
    }
    // The last element of the vector must be NULL for exec to accept it.
    cstring_vec.push_back(NULL);
  }
};

template <class T>
class JsonArgParser {
 public:
  const google::scp::core::ExecutionResult Parse(std::string json_string,
                                                 T& parsed_value) noexcept {
    return google::scp::core::FailureExecutionResult(
        google::scp::core::errors::ARGUMENT_PARSER_UNKNOWN_TYPE);
  };
};

template <>
class JsonArgParser<ExecutableArgument> {
 public:
  const google::scp::core::ExecutionResult Parse(
      std::string json_string, ExecutableArgument& parsed_value) noexcept {
    try {
      auto parsed = nlohmann::json::parse(json_string);

      if (!parsed.contains("executable_name")) {
        return google::scp::core::FailureExecutionResult(
            google::scp::core::errors::ARGUMENT_PARSER_INVALID_EXEC_ARG_JSON);
      }

      parsed_value.executable_name =
          parsed["executable_name"].get<std::string>();
      for (auto& arg : parsed["command_line_args"]) {
        parsed_value.command_line_args.push_back(arg);
      }

      if (parsed.contains("restart")) {
        parsed_value.restart = parsed["restart"].get<bool>();
      }
    } catch (std::exception& e) {
      std::cerr << "Failed parsing json with: " << e.what() << std::endl;
      parsed_value.executable_name = std::string();
      parsed_value.command_line_args.clear();
      return google::scp::core::FailureExecutionResult(
          google::scp::core::errors::ARGUMENT_PARSER_INVALID_JSON);
    }

    return google::scp::core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::process_launcher
