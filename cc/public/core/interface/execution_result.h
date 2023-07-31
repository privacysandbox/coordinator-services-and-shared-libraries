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

#ifndef SCP_CORE_INTERFACE_EXECUTION_RESULT_H_
#define SCP_CORE_INTERFACE_EXECUTION_RESULT_H_

#include <variant>

#include "core/common/proto/common.pb.h"

namespace google::scp::core {

#define RETURN_IF_FAILURE(__execution_result)                            \
  if (ExecutionResult __res = __execution_result; !__res.Successful()) { \
    return __res;                                                        \
  }

/// Operation's execution status.
enum class ExecutionStatus {
  /// Executed successfully.
  Success = 0,
  /// Execution failed.
  Failure = 1,
  /// did not execution and requires retry.
  Retry = 2,
};

/// Convert ExecutionStatus to Proto.
core::common::proto::ExecutionStatus ToStatusProto(ExecutionStatus& status);

/// Status code returned from operation execution.
typedef uint64_t StatusCode;
#define SC_OK 0UL
#define SC_UNKNOWN 1UL

struct ExecutionResult;
constexpr ExecutionResult SuccessExecutionResult();

/// Operation's execution result including status and status code.
struct ExecutionResult {
  /**
   * @brief Construct a new Execution Result object
   *
   * @param status status of the execution.
   * @param status_code code of the execution status.
   */
  constexpr ExecutionResult(ExecutionStatus status, StatusCode status_code)
      : status(status), status_code(status_code) {}

  constexpr ExecutionResult()
      : ExecutionResult(ExecutionStatus::Failure, SC_UNKNOWN) {}

  explicit ExecutionResult(
      const core::common::proto::ExecutionResult result_proto);

  bool operator==(const ExecutionResult& other) const {
    return status == other.status && status_code == other.status_code;
  }

  bool operator!=(const ExecutionResult& other) const {
    return !operator==(other);
  }

  core::common::proto::ExecutionResult ToProto();

  bool Successful() const { return *this == SuccessExecutionResult(); }

  explicit operator bool() const { return Successful(); }

  /// Status of the executed operation.
  ExecutionStatus status = ExecutionStatus::Failure;
  /**
   * @brief if the operation was not successful, status_code will indicate the
   * error code.
   */
  StatusCode status_code = SC_UNKNOWN;
};

/// ExecutionResult with success status
constexpr ExecutionResult SuccessExecutionResult() {
  return ExecutionResult(ExecutionStatus::Success, SC_OK);
}

/// ExecutionResult with failure status
class FailureExecutionResult : public ExecutionResult {
 public:
  /// Construct a new Failure Execution Result object
  explicit constexpr FailureExecutionResult(StatusCode status_code)
      : ExecutionResult(ExecutionStatus::Failure, status_code) {}
};

/// ExecutionResult with retry status
class RetryExecutionResult : public ExecutionResult {
 public:
  /// Construct a new Retry Execution Result object
  explicit RetryExecutionResult(StatusCode status_code)
      : ExecutionResult(ExecutionStatus::Retry, status_code) {}
};

// Wrapper class to allow for returning either an ExecutionResult or a value.
// Example use with a function:
//
// ExecutionResultOr<int> ConvertStringToInt(std::string str) {
//   if (str is not an int) {
//     return ExecutionResult(Failure, <some_error_code>);
//   }
//   return string_to_int(str);
// }
//
// NOTE: The type T should be copyable and moveable.
template <typename T>
class ExecutionResultOr : public std::variant<ExecutionResult, T> {
 private:
  using base = std::variant<ExecutionResult, T>;

 public:
  using base::base;
  using base::operator=;

  ExecutionResultOr() : base(ExecutionResult()) {}

  ///////////// ExecutionResult methods ///////////////////////////////////////

  // Returns true if this contains a value - i.e. it does not contain a status.
  bool Successful() const { return result().Successful(); }

  // Returns the ExecutionResult (if contained), Success otherwise.
  ExecutionResult result() const {
    if (!HasExecutionResult()) {
      return SuccessExecutionResult();
    }
    return std::get<ExecutionResult>(*this);
  }

  ///////////// Value methods /////////////////////////////////////////////////

  // Returns true if this contains a value.
  bool has_value() const { return !HasExecutionResult(); }

  // Returns the value held by this.
  // Should be guarded by has_value() calls - otherwise the behavior is
  // undefined.
  const T& value() const { return std::get<T>(*this); }
  T& value() { return std::get<T>(*this); }
  const T& operator*() const { return std::get<T>(*this); }
  T& operator*() { return std::get<T>(*this); }

  // Returns a pointer to the value held by this.
  // Returns nullptr if no value is contained.
  const T* operator->() const { return std::get_if<T>(this); }
  T* operator->() { return std::get_if<T>(this); }

 private:
  bool HasExecutionResult() const {
    return std::holds_alternative<ExecutionResult>(*this);
  }
};

}  // namespace google::scp::core

#endif  // SCP_CORE_INTERFACE_EXECUTION_RESULT_H_
