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

#include <functional>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "include/v8.h"
#include "roma/common/src/containers.h"

#include "type_converter.h"

namespace google::scp::roma {
/**
 * @brief Base class for a function binding object.
 * A function binding object represents a JS/C++ function binding. That is,
 * it registers a C++ function that gets called when a given JS function is
 * called.
 */
class FunctionBindingObjectBase {
 public:
  FunctionBindingObjectBase()
      : Signature(FunctionBindingObjectBase::KnownSignature) {}

  static const int KnownSignature = 0x5A5A5A5A5A5A5A5A;
  /**
   * @brief This object is expected to be cast in a somewhat unsafe manner
   * using reinterpret_cast. So we add this property to check against when
   * a generic pointer is casted back to this type.
   */
  int Signature;

  virtual ~FunctionBindingObjectBase() {}

  /**
   * @brief Get the function name
   *
   * @return std::string
   */
  virtual std::string GetFunctionName() = 0;

  /**
   * @brief Call the C++ handler when the function is called from JS
   *
   * @param info The parameter passed by v8 on function call
   */
  virtual void InvokeInternalHandler(
      const v8::FunctionCallbackInfo<v8::Value>& info) = 0;
};

/**
 * @brief Class representing a C++/JS function binding.
 * template <typename TReturn, typename... TInputs>
 * @tparam TReturn The output of the function.
 * @tparam TInputs The inputs of the function.
 */
template <typename TReturn, typename... TInputs>
class FunctionBindingObject : public FunctionBindingObjectBase {
 public:
  /**
   * @brief The name by which Javascript code can call this function.
   */
  std::string function_name;

  /**
   * @brief The function that will be bound to a Javascript function.
   * @param inputs The inputs passed to the function - what will be passed as
   * arguments to the JS function.
   */
  std::function<TReturn(std::tuple<TInputs...>& inputs)> function;

  std::string GetFunctionName() override { return function_name; }

 protected:
  /**
   * @brief This function attempts to convert the JS types passed in the
   * FunctionCallbackInfo object into native C++ types. It then calls the
   * user-provided function, and gets the output. It tries to convert
   * said output into a JS type so that it can be returned by the JS function
   * that was bound to the user-provided C++ function.
   *
   * @param info The FunctionCallbackInfo that is passed to C++/JS binding from
   * v8
   */
  void InvokeInternalHandler(
      const v8::FunctionCallbackInfo<v8::Value>& info) override {
    constexpr size_t num_inputs = sizeof...(TInputs);

    // Get the isolate
    v8::Isolate* isolate = info.GetIsolate();
    v8::HandleScope handle_scope(isolate);

    if (info.Length() != num_inputs) {
      auto error_message =
          "(" + function_name + ") Unexpected number of inputs";
      isolate->ThrowError(
          TypeConverter<std::string>::ToV8(isolate, error_message)
              .As<v8::String>());
      return;
    }

    bool conversion_worked = true;

    // Tuple that will be passed as input to the user-provided C++ function
    std::tuple<TInputs...> arguments_tuple;

    constexpr_for<0>([&info, &isolate, &conversion_worked,
                      &arguments_tuple](auto i) {
      using TGivenInput =
          typename std::tuple_element<i, std::tuple<TInputs...>>::type;

      constexpr bool output_types_are_only_allowed_ones =
          std::is_same<std::string, TReturn>::value ||
          std::is_same<std::vector<std::string>, TReturn>::value ||
          std::is_same<common::Map<std::string, std::string>, TReturn>::value;

      constexpr bool input_types_are_only_allowed_ones =
          std::is_same<std::string, TGivenInput>::value ||
          std::is_same<std::vector<std::string>, TGivenInput>::value ||
          std::is_same<common::Map<std::string, std::string>,
                       TGivenInput>::value;

      // We only allow these types as output for now
      static_assert(output_types_are_only_allowed_ones,
                    "Only allowed output types are std::string and "
                    "std::vector<std::string> and "
                    "roma::common::Map<std::string, std::string>");

      // We only allow these types as input for now
      static_assert(input_types_are_only_allowed_ones,
                    "Only allowed input types are std::string and "
                    "std::vector<std::string> and "
                    "roma::common::Map<std::string, std::string>");

      TGivenInput value;
      if (!TypeConverter<TGivenInput>::FromV8(isolate, info[i], &value)) {
        conversion_worked = false;
      } else {
        std::get<i>(arguments_tuple) = value;
      }
    });

    if (!conversion_worked) {
      auto error_message =
          "(" + function_name + ") Error encountered while converting types";
      isolate->ThrowError(
          TypeConverter<std::string>::ToV8(isolate, error_message)
              .As<v8::String>());
      return;
    }

    // Call the user-provided function and capture the output
    TReturn function_output;
    function_output = function(arguments_tuple);
    // Convert the output to a JS type so that it can be returned by the JS
    // function
    auto js_output = TypeConverter<TReturn>::ToV8(isolate, function_output);
    info.GetReturnValue().Set(js_output);
  }

  // Utility to generate a compile-time for loop for the input types.
  // It unwraps the types, which are known at compile time so that all the
  // elements can be accessed and used.
  template <auto i, typename F>
  constexpr void constexpr_for(F&& f) {
    if constexpr (i < sizeof...(TInputs)) {
      f(std::integral_constant<decltype(i), i>());
      constexpr_for<i + 1>(f);
    }
  }
};
}  // namespace google::scp::roma
