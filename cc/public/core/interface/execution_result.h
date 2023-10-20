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

#include <utility>
#include <variant>

#include "core/common/proto/common.pb.h"

namespace google::scp::core {

// Macro to shorten this pattern:
// ExecutionResult result = foo();
// if (!result.Successful()) {
//   return result;
// }
//
// This is useful if the callsite doesn't need to use the ExecutionResult
// anymore than just returning it upon failure.
//
// Example 1:
// ExecutionResult result = foo();
// RETURN_IF_FAILURE(result);
// // If we reach this point, result was Successful.
//
// Example 2:
// RETURN_IF_FAILURE(foo());
// // If we reach this point, foo() was Successful.
#define RETURN_IF_FAILURE(execution_result)                          \
  if (::google::scp::core::ExecutionResult __res = execution_result; \
      !__res.Successful()) {                                         \
    return __res;                                                    \
  }

// Same as above but logs an error before returning upon failure.
// The other arguments would be the same as those used in SCP_ERROR(...) except
// the ExecutionResult is abstracted away.
//
// Example:
// RETURN_AND_LOG_IF_FAILURE(foo(), kComponentName, parent_activity_id,
//     activity_id, "some message %s", str.c_str()));
// // If we reach this point, foo() was Successful and SCP_ERROR_CONTEXT was not
// // called.
#define RETURN_AND_LOG_IF_FAILURE(execution_result, component_name,    \
                                  activity_id, message, ...)           \
  __RETURN_IF_FAILURE_LOG(execution_result, SCP_ERROR, component_name, \
                          activity_id, message, ##__VA_ARGS__)
// Same as above but logs an error using the supplied context upon failure.
// The other arguments would be the same as those used in
// SCP_ERROR_CONTEXT(...). RETURN_AND_LOG_IF_FAILURE_CONTEXT(foo(),
// kComponentName, context, "some message %s", str.c_str()));
#define RETURN_AND_LOG_IF_FAILURE_CONTEXT(execution_result, component_name,    \
                                          async_context, message, ...)         \
  __RETURN_IF_FAILURE_LOG(execution_result, SCP_ERROR_CONTEXT, component_name, \
                          async_context, message, ##__VA_ARGS__)

#define __RETURN_IF_FAILURE_LOG(execution_result, error_level, component_name, \
                                activity_id, message, ...)                     \
  if (::google::scp::core::ExecutionResult __res = execution_result;           \
      !__res.Successful()) {                                                   \
    error_level(component_name, activity_id, __res, message, ##__VA_ARGS__);   \
    return __res;                                                              \
  }

// Macro similar to RETURN_IF_FAILURE but for ExecutionResultOr.
// Useful for shortening this pattern:
// ExecutionResultOr<Foo> result_or = foo();
// if (!result_or.Successful()) {
//   return result_or.result();
// }
// Foo val = std::move(*result_or);
//
// Example 1:
// // NOTEs:
// // 1. This pattern will not compile if Foo is non-copyable - use Example 2
// //    instead.
// // 2. This pattern results in the value being copied once into an internal
// //    variable.
//
// ExecutionResultOr<Foo> result_or = foo();
// ASSIGN_OR_RETURN(auto val, result_or);
// // If we reach this point, foo() was Successful and val is of type Foo.
//
// Example 2:
// ASSIGN_OR_RETURN(auto val, foo());
// // If we reach this point, foo() was Successful and val is of type Foo.
//
// Example 3:
// Foo val;
// ASSIGN_OR_RETURN(val, foo());
// // If we reach this point, foo() was Successful and val holds the value.
//
// Example 4:
// std::pair<Foo, Bar> pair;
// ASSIGN_OR_RETURN(pair.first, foo());
// // If we reach this point, foo() was Successful and pair.first holds the
// // value.
#define ASSIGN_OR_RETURN(lhs, execution_result_or)            \
  __ASSIGN_OR_RETURN_HELPER(lhs, __UNIQUE_VAR_NAME(__LINE__), \
                            execution_result_or, )

// Same as above but logs the error before returning it.
// The other arguments would be the same as those used in SCP_ERROR(...) except
// the ExecutionResult is abstracted away.
//
// Example:
// ExecutionResultOr<Foo> result_or = foo();
// ASSIGN_OR_LOG_AND_RETURN(auto val, result_or, kComponentName,
//     parent_activity_id, activity_id, "some message %s", str.c_str());
// // If we reach this point, foo() was Successful and val is of type Foo.
#define ASSIGN_OR_LOG_AND_RETURN(lhs, execution_result_or, component_name, \
                                 activity_id, message, ...)                \
  __ASSIGN_OR_RETURN_HELPER(                                               \
      lhs, __UNIQUE_VAR_NAME(__LINE__), execution_result_or,               \
      SCP_ERROR(component_name, activity_id, __res, message, ##__VA_ARGS__))

// Same as above but logs the error using the supplied context before returning
// it.
// The other arguments would be the same as those used in
// SCP_ERROR_CONTEXT(...) except the ExecutionResult is abstracted away.
//
// Example:
// ExecutionResultOr<Foo> result_or = foo();
// ASSIGN_OR_LOG_AND_RETURN_CONTEXT(auto val, result_or, kComponentName,
//     context, "some message %s", str.c_str());
// // If we reach this point, foo() was Successful and val is of type Foo.
#define ASSIGN_OR_LOG_AND_RETURN_CONTEXT(                                    \
    lhs, execution_result_or, component_name, async_context, message, ...)   \
  __ASSIGN_OR_RETURN_HELPER(lhs, __UNIQUE_VAR_NAME(__LINE__),                \
                            execution_result_or,                             \
                            SCP_ERROR_CONTEXT(component_name, async_context, \
                                              __res, message, ##__VA_ARGS__))

#define __ASSIGN_OR_RETURN_W_LOG(lhs, execution_result_or, failure_statement) \
  __ASSIGN_OR_RETURN_HELPER(lhs, __UNIQUE_VAR_NAME(__LINE__),                 \
                            execution_result_or, failure_statement)

#define __UNIQUE_VAR_NAME_HELPER(x, y) x##y
#define __UNIQUE_VAR_NAME(x) __UNIQUE_VAR_NAME_HELPER(__var, x)

#define __ASSIGN_OR_RETURN_HELPER(lhs, result_or_temp_var_name,           \
                                  execution_result_or, failure_statement) \
  auto&& result_or_temp_var_name = execution_result_or;                   \
  if (!result_or_temp_var_name.Successful()) {                            \
    auto __res = result_or_temp_var_name.result();                        \
    failure_statement;                                                    \
    return __res;                                                         \
  }                                                                       \
  lhs = result_or_temp_var_name.release();

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

  bool Retryable() const { return this->status == ExecutionStatus::Retry; }

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
// NOTE 1: The type T should be copyable and moveable.
// NOTE 2: After moving the value out of an ExecutionResultOr result_or, the
// value in the result_or will be the same as the value v after:
// Foo w(std::move(v));
//
// i.e. use-after-move still applies, but result_or.Successful() would still be
// true.
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

  // clang-format off

  // Returns the value held by this.
  // Should be guarded by has_value() calls - otherwise the behavior is
  // undefined.
  const T& value() const& { return std::get<T>(*this); }
  // lvalue reference overload - indicated by "...() &".
  T& value() & { return std::get<T>(*this); }
  // rvalue reference overload - indicated by "...() &&".
  T&& value() && { return std::move(this->value()); }

  const T& operator*() const& { return this->value(); }
  // lvalue reference overload - indicated by "...() &".
  T& operator*() & { return this->value(); }
  // rvalue reference overload - indicated by "...() &&".
  T&& operator*() && { return std::move(this->value()); }

  // Returns a pointer to the value held by this.
  // Returns nullptr if no value is contained.
  const T* operator->() const { return std::get_if<T>(this); }
  T* operator->() { return std::get_if<T>(this); }

  // alias for value() && but no call to std::move() is necessary.
  T&& release() { return std::move(this->value()); }

  // clang-format on

 private:
  bool HasExecutionResult() const {
    return std::holds_alternative<ExecutionResult>(*this);
  }
};

}  // namespace google::scp::core

#endif  // SCP_CORE_INTERFACE_EXECUTION_RESULT_H_
