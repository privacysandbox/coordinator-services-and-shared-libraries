/*
 * Copyright 2023 Google LLC
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

#include "v8_isolate_visitor_function_binding.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "cc/roma/interface/function_binding_io.pb.h"
#include "roma/common/src/containers.h"
#include "roma/config/src/type_converter.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/constants/constants.h"

#include "error_codes.h"

using absl::flat_hash_map;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_ROMA_V8_ENGINE_COULD_NOT_REGISTER_FUNCTION_BINDING;
using google::scp::core::errors::
    SC_ROMA_V8_ISOLATE_VISITOR_FUNCTION_BINDING_EMPTY_CONTEXT;
using google::scp::core::errors::
    SC_ROMA_V8_ISOLATE_VISITOR_FUNCTION_BINDING_INVALID_ISOLATE;
using google::scp::roma::proto::FunctionBindingIoProto;
using google::scp::roma::sandbox::constants::kMetadataRomaRequestId;
using std::make_unique;
using std::string;
using std::to_string;
using std::vector;
using v8::Array;
using v8::Context;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Map;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Uint8Array;
using v8::Undefined;
using v8::Value;

static constexpr char kCouldNotRunFunctionBinding[] =
    "ROMA: Could not run C++ function binding.";
static constexpr char kUnexpectedDataInBindingCallback[] =
    "ROMA: Unexpected data in global callback.";
static constexpr char kCouldNotConvertJsFunctionInputToNative[] =
    "ROMA: Could not convert JS function input to native C++ type.";
static constexpr char kErrorInFunctionBindingInvocation[] =
    "ROMA: Error while executing native function binding.";

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {
static bool V8TypesToProto(const FunctionCallbackInfo<Value>& info,
                           FunctionBindingIoProto& proto) {
  if (info.Length() == 0) {
    // No arguments were passed to function
    return true;
  }
  if (info.Length() > 1) {
    return false;
  }

  auto isolate = info.GetIsolate();
  auto function_parameter = info[0];

  // Try to convert to one of the supported types
  string string_native;
  vector<string> vector_of_string_native;
  flat_hash_map<string, string> map_of_string_native;

  if (TypeConverter<string>::FromV8(isolate, function_parameter,
                                    &string_native)) {
    proto.set_input_string(string_native);
  } else if (TypeConverter<vector<string>>::FromV8(isolate, function_parameter,
                                                   &vector_of_string_native)) {
    proto.mutable_input_list_of_string()->mutable_data()->Add(
        vector_of_string_native.begin(), vector_of_string_native.end());
  } else if (TypeConverter<flat_hash_map<string, string>>::FromV8(
                 isolate, function_parameter, &map_of_string_native)) {
    for (auto&& kvp : map_of_string_native) {
      (*proto.mutable_input_map_of_string()->mutable_data())[kvp.first] =
          kvp.second;
    }
  } else if (function_parameter->IsUint8Array()) {
    auto array = function_parameter.As<Uint8Array>();
    auto data_len = array->Length();
    auto native_data = make_unique<uint8_t[]>(data_len);
    if (!TypeConverter<uint8_t*>::FromV8(isolate, function_parameter,
                                         native_data.get(), data_len)) {
      return false;
    }

    proto.set_input_bytes(native_data.get(), data_len);
  } else {
    // Unknown type
    return false;
  }

  return true;
}

static Local<Value> ProtoToV8Type(Isolate* isolate,
                                  const FunctionBindingIoProto& proto) {
  if (proto.has_output_string()) {
    return TypeConverter<string>::ToV8(isolate, proto.output_string());
  } else if (proto.has_output_list_of_string()) {
    auto len = proto.output_list_of_string().data().size();
    vector<string> output_list;
    for (int i = 0; i < len; i++) {
      output_list.push_back(proto.output_list_of_string().data().at(i));
    }
    return TypeConverter<vector<string>>::ToV8(isolate, output_list);
  } else if (proto.has_output_map_of_string()) {
    flat_hash_map<string, string> output_map;
    for (auto& [key, value] : proto.output_map_of_string().data()) {
      output_map[key] = value;
    }
    return TypeConverter<flat_hash_map<string, string>>::ToV8(isolate,
                                                              output_map);
  } else if (proto.has_output_bytes()) {
    const auto& bytes = proto.output_bytes();
    return TypeConverter<uint8_t*>::ToV8(
        isolate, reinterpret_cast<const uint8_t*>(bytes.data()),
        bytes.length());
  }

  // This function didn't return anything from C++
  ROMA_VLOG(1) << "Function binding did not set any return value from C++.";
  return Undefined(isolate);
}

void V8IsolateVisitorFunctionBinding::GlobalV8FunctionCallback(
    const FunctionCallbackInfo<Value>& info) {
  auto isolate = info.GetIsolate();
  auto context = isolate->GetCurrentContext();
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);

  auto data = info.Data();

  if (data.IsEmpty()) {
    isolate->ThrowError(kUnexpectedDataInBindingCallback);
    return;
  }

  Local<External> binding_info_pair_external = Local<External>::Cast(data);
  auto binding_info_pair =
      reinterpret_cast<BindingPair*>(binding_info_pair_external->Value());

  FunctionBindingIoProto function_invocation_proto;
  if (!V8TypesToProto(info, function_invocation_proto)) {
    isolate->ThrowError(kCouldNotConvertJsFunctionInputToNative);
    return;
  }

  // Read the request ID from the global object in the context
  auto request_id_label =
      TypeConverter<string>::ToV8(isolate, kMetadataRomaRequestId).As<String>();
  auto roma_request_id_maybe =
      context->Global()->Get(context, request_id_label);
  Local<Value> roma_request_id;
  string roma_request_id_native;
  if (roma_request_id_maybe.ToLocal(&roma_request_id) &&
      TypeConverter<string>::FromV8(isolate, roma_request_id,
                                    &roma_request_id_native)) {
    // Set the request ID in the function call metadata so that it is accessible
    // when the function is invoked in the user-provided binding.
    (*function_invocation_proto.mutable_metadata())[kMetadataRomaRequestId] =
        roma_request_id_native;
  } else {
    LOG(ERROR) << "Could not read request ID from metadata in hook.";
  }

  string native_function_name = binding_info_pair->first;
  if (native_function_name.empty()) {
    isolate->ThrowError(kCouldNotRunFunctionBinding);
    return;
  }

  auto result = binding_info_pair->second->function_invoker_->Invoke(
      native_function_name, function_invocation_proto);
  if (!result.Successful()) {
    isolate->ThrowError(kCouldNotRunFunctionBinding);
    return;
  }
  if (function_invocation_proto.errors().size() > 0) {
    isolate->ThrowError(kErrorInFunctionBindingInvocation);
    return;
  }

  auto returned_value = ProtoToV8Type(isolate, function_invocation_proto);
  info.GetReturnValue().Set(returned_value);
}

ExecutionResult V8IsolateVisitorFunctionBinding::Visit(
    Isolate* isolate, Local<ObjectTemplate>& global_object_template) noexcept {
  if (!isolate) {
    return FailureExecutionResult(
        SC_ROMA_V8_ISOLATE_VISITOR_FUNCTION_BINDING_INVALID_ISOLATE);
  }

  for (auto binding_refer : binding_references_) {
    // Store a pointer to this V8IsolateVisitorFunctionBinding instance
    Local<External> binding_context_pair =
        External::New(isolate, reinterpret_cast<void*>(binding_refer.get()));

    // Create the function template to register in the global object
    auto function_template = FunctionTemplate::New(
        isolate, &GlobalV8FunctionCallback, binding_context_pair);

    // Convert the function binding name to a v8 type
    auto binding_name =
        TypeConverter<string>::ToV8(isolate, binding_refer->first).As<String>();

    global_object_template->Set(binding_name, function_template);
  }

  return SuccessExecutionResult();
}

void V8IsolateVisitorFunctionBinding::AddExternalReferences(
    std::vector<intptr_t>& external_references) noexcept {
  // Must add pointers that are not within the v8 heap to external_references_
  // so that the snapshot serialization works.
  for (auto binding_refer : binding_references_) {
    external_references.push_back(
        reinterpret_cast<intptr_t>(binding_refer.get()));
  }
  external_references.push_back(
      reinterpret_cast<intptr_t>(&GlobalV8FunctionCallback));
}

}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine
