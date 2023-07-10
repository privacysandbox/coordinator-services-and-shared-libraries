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

#include "execution_utils.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "include/v8.h"
#include "public/core/interface/execution_result.h"
#include "roma/config/src/type_converter.h"
#include "roma/ipc/src/ipc_message.h"
#include "roma/wasm/src/deserializer.h"
#include "roma/wasm/src/serializer.h"
#include "roma/wasm/src/wasm_types.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ROMA_V8_WORKER_BAD_HANDLER_NAME;
using google::scp::core::errors::SC_ROMA_V8_WORKER_BAD_INPUT_ARGS;
using google::scp::core::errors::SC_ROMA_V8_WORKER_BAD_SOURCE_CODE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION;
using google::scp::core::errors::SC_ROMA_V8_WORKER_RESULT_PARSE_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_SCRIPT_EXECUTION_TIMEOUT;
using google::scp::core::errors::SC_ROMA_V8_WORKER_SCRIPT_RUN_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE;
using google::scp::core::errors::
    SC_ROMA_V8_WORKER_WASM_OBJECT_RETRIEVAL_FAILURE;
using google::scp::roma::common::RomaString;
using google::scp::roma::common::RomaVector;
using google::scp::roma::ipc::RomaCodeObj;
using google::scp::roma::wasm::RomaWasmListOfStringRepresentation;
using google::scp::roma::wasm::RomaWasmStringRepresentation;
using google::scp::roma::wasm::WasmDeserializer;
using google::scp::roma::wasm::WasmSerializer;
using std::string;
using std::vector;
using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallback;
using v8::FunctionCallbackInfo;
using v8::Global;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::JSON;
using v8::Local;
using v8::MemorySpan;
using v8::Message;
using v8::Name;
using v8::NewStringType;
using v8::Object;
using v8::ObjectTemplate;
using v8::Script;
using v8::ScriptCompiler;
using v8::String;
using v8::TryCatch;
using v8::UnboundScript;
using v8::Undefined;
using v8::Value;
using v8::WasmMemoryObject;
using v8::WasmModuleObject;

namespace google::scp::roma::worker {
static constexpr char kWebAssemblyTag[] = "WebAssembly";
static constexpr char kInstanceTag[] = "Instance";
static constexpr char kExportsTag[] = "exports";
static constexpr char kRegisteredWasmExports[] = "RomaRegisteredWasmExports";
static constexpr char kWasmMemory[] = "memory";
static constexpr char kWasiSnapshotPreview[] = "wasi_snapshot_preview1";
static constexpr char kWasiProcExitFunctionName[] = "proc_exit";
static constexpr char kTimeoutErrorMsg[] = "execution timeout";

// TODO: maybe more to TypeConversion folder.
bool ExecutionUtils::RomaStrToLocalStr(const RomaString& roma_string,
                                       Local<String>& v8_string) noexcept {
  auto string_maybe =
      String::NewFromUtf8(Isolate::GetCurrent(), roma_string.c_str(),
                          NewStringType::kNormal, roma_string.length());
  return string_maybe.ToLocal(&v8_string);
}

/**
 * @brief Parse the input using JSON::Parse to turn it into the right JS types
 *
 * @param input
 * @return Local<Array> The array of parsed values
 */
static Local<Array> ParseAsJsInput(const RomaVector<RomaString>& input) {
  auto isolate = Isolate::GetCurrent();
  auto context = isolate->GetCurrentContext();

  const int argc = input.size();

  Local<Array> argv = Array::New(isolate, argc);
  for (auto i = 0; i < argc; ++i) {
    Local<String> arg_str;
    if (!ExecutionUtils::RomaStrToLocalStr(input[i], arg_str)) {
      return Local<Array>();
    }

    Local<Value> arg = Undefined(isolate);
    if (arg_str->Length() && !JSON::Parse(context, arg_str).ToLocal(&arg)) {
      return Local<Array>();
    }
    if (!argv->Set(context, i, arg).ToChecked()) {
      return Local<Array>();
    }
  }

  return argv;
}

/**
 * @brief Get the WASM memory object that was registered in the global context
 *
 * @param isolate
 * @param context
 * @return Local<Value> The WASM memory object
 */
static Local<Value> GetWasmMemoryObject(Isolate* isolate,
                                        Local<Context>& context) {
  auto wasm_exports = context->Global()
                          ->Get(context, TypeConverter<std::string>::ToV8(
                                             isolate, kRegisteredWasmExports))
                          .ToLocalChecked()
                          .As<Object>();

  auto wasm_memory_maybe = wasm_exports->Get(
      context, TypeConverter<std::string>::ToV8(isolate, kWasmMemory));

  if (wasm_memory_maybe.IsEmpty()) {
    return Undefined(isolate);
  }

  return wasm_memory_maybe.ToLocalChecked();
}

/**
 * @brief Parse the handler input to be provided to a WASM handler.
 * This function handles writing to the WASM memory if necessary.
 *
 * @param isolate
 * @param context
 * @param input
 * @return Local<Array> The input arguments to be provided to the WASM handler
 */
static Local<Array> ParseAsWasmInput(Isolate* isolate, Local<Context>& context,
                                     const RomaVector<RomaString>& input) {
  // Parse it into JS types so we can distinguish types
  auto parsed_args = ParseAsJsInput(input).As<Array>();
  const int argc = parsed_args.As<Array>()->Length();
  Local<Array> argv = Array::New(isolate, argc);

  auto wasm_memory = GetWasmMemoryObject(isolate, context);

  if (wasm_memory->IsUndefined()) {
    // The module has not memory object. This is either a very basic WASM, or
    // invalid, we'll just exit early, and pass the input as it was parsed.
    return parsed_args;
  }

  auto wasm_memory_blob =
      wasm_memory.As<WasmMemoryObject>()->Buffer()->GetBackingStore()->Data();
  auto wasm_memory_size = wasm_memory.As<WasmMemoryObject>()
                              ->Buffer()
                              ->GetBackingStore()
                              ->ByteLength();

  size_t wasm_memory_offset = 0;

  for (auto i = 0; i < argc; ++i) {
    auto arg = parsed_args->Get(context, i).ToLocalChecked();

    // We only support uint/int, string and array of string args
    if (!arg->IsUint32() && !arg->IsInt32() && !arg->IsString() &&
        !arg->IsArray()) {
      argv.Clear();
      return Local<Array>();
    }

    Local<Value> new_arg;

    if (arg->IsUint32() || arg->IsInt32()) {
      // No serialization needed
      new_arg = arg;
    }
    if (arg->IsString()) {
      string str_value;
      TypeConverter<string>::FromV8(isolate, arg, &str_value);
      auto string_ptr_in_wasm_memory = WasmSerializer::WriteCustomString(
          wasm_memory_blob, wasm_memory_size, wasm_memory_offset, str_value);

      // The serialization failed
      if (string_ptr_in_wasm_memory == UINT32_MAX) {
        return Local<Array>();
      }

      new_arg =
          TypeConverter<uint32_t>::ToV8(isolate, string_ptr_in_wasm_memory);
      wasm_memory_offset +=
          RomaWasmStringRepresentation::ComputeMemorySizeFor(str_value);
    }
    if (arg->IsArray()) {
      vector<string> vec_value;
      bool worked =
          TypeConverter<vector<string>>::FromV8(isolate, arg, &vec_value);

      if (!worked) {
        // This means the array is not an array of string
        return Local<Array>();
      }

      auto list_ptr_in_wasm_memory = WasmSerializer::WriteCustomListOfString(
          wasm_memory_blob, wasm_memory_size, wasm_memory_offset, vec_value);

      // The serialization failed
      if (list_ptr_in_wasm_memory == UINT32_MAX) {
        return Local<Array>();
      }

      new_arg = TypeConverter<uint32_t>::ToV8(isolate, list_ptr_in_wasm_memory);
      wasm_memory_offset +=
          RomaWasmListOfStringRepresentation::ComputeMemorySizeFor(vec_value);
    }

    if (!argv->Set(context, i, new_arg).ToChecked()) {
      return Local<Array>();
    }
  }

  return argv;
}

Local<Array> ExecutionUtils::InputToLocalArgv(
    const RomaVector<RomaString>& input, bool is_wasm) noexcept {
  auto isolate = Isolate::GetCurrent();
  auto context = isolate->GetCurrentContext();

  if (is_wasm) {
    return ParseAsWasmInput(isolate, context, input);
  }

  return ParseAsJsInput(input);
}

static string DescribeError(Isolate* isolate, TryCatch* try_catch) noexcept {
  auto context = isolate->GetCurrentContext();

  auto exception = try_catch->Exception();
  if (exception.IsEmpty()) {
    return std::string();
  }

  Local<String> local_exception;
  if (!exception->ToString(context).ToLocal(&local_exception)) {
    return std::string();
  }
  string exception_msg;
  TypeConverter<string>::FromV8(isolate, local_exception, &exception_msg);

  const Local<Message> message = try_catch->Message();

  // If there's no message, just return the exception_msg.
  if (message.IsEmpty()) {
    return exception_msg;
  }

  // We want to return a message of the form:
  //
  //     7: ReferenceError: blah is not defined.
  //
  int line;
  // Sometimes for multi-line errors there is no line number.
  if (!message->GetLineNumber(isolate->GetCurrentContext()).To(&line)) {
    return absl::StrFormat("%s", exception_msg);
  }

  return absl::StrFormat("%i: %s", line, exception_msg);
}

ExecutionResult ExecutionUtils::ReportException(TryCatch* try_catch,
                                                RomaString& err_msg) noexcept {
  auto isolate = Isolate::GetCurrent();
  HandleScope handle_scope(isolate);
  String::Utf8Value exception(isolate, try_catch->Exception());

  // Checks isolate is currently terminating because of a call to
  // TerminateExecution.
  if (isolate->IsExecutionTerminating()) {
    err_msg = RomaString(kTimeoutErrorMsg);
    return FailureExecutionResult(SC_ROMA_V8_WORKER_SCRIPT_EXECUTION_TIMEOUT);
  }

  err_msg = DescribeError(isolate, try_catch);

  return FailureExecutionResult(SC_UNKNOWN);
}

ExecutionResult ExecutionUtils::CompileRunJS(
    const RomaString& js, RomaString& err_msg,
    Local<UnboundScript>* unbound_script) noexcept {
  auto isolate = Isolate::GetCurrent();
  TryCatch try_catch(isolate);
  Local<Context> context(isolate->GetCurrentContext());

  Local<String> js_source;
  if (!ExecutionUtils::RomaStrToLocalStr(js, js_source)) {
    // ReportException just used to catch exception for err_msg. No
    // manually isolate TerminateExecution() call in CompileRunJS(), so don't
    // need to convert the failure result with GetExecutionResult();
    ReportException(&try_catch, err_msg);
    return FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_SOURCE_CODE);
  }

  Local<Script> script;
  if (!Script::Compile(context, js_source).ToLocal(&script)) {
    ReportException(&try_catch, err_msg);
    return FailureExecutionResult(SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE);
  }

  if (unbound_script) {
    *unbound_script = script->GetUnboundScript();
  }

  Local<Value> script_result;
  if (!script->Run(context).ToLocal(&script_result)) {
    ReportException(&try_catch, err_msg);
    return FailureExecutionResult(SC_ROMA_V8_WORKER_SCRIPT_RUN_FAILURE);
  }

  return SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::GetJsHandler(const RomaString& handler_name,
                                             Local<Value>& handler,
                                             RomaString& err_msg) noexcept {
  if (handler_name.empty()) {
    return FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_HANDLER_NAME);
  }
  auto isolate = Isolate::GetCurrent();
  TryCatch try_catch(isolate);
  Local<Context> context(isolate->GetCurrentContext());

  // Fetch out the handler name from code object.
  Local<String> local_name;
  if (!ExecutionUtils::RomaStrToLocalStr(handler_name, local_name)) {
    auto exception_result = ReportException(&try_catch, err_msg);
    return GetExecutionResult(exception_result,
                              SC_ROMA_V8_WORKER_BAD_HANDLER_NAME);
  }

  // If there is no handler function, or if it is not a function,
  // bail out
  if (!context->Global()->Get(context, local_name).ToLocal(&handler) ||
      !handler->IsFunction()) {
    auto exception_result = ReportException(&try_catch, err_msg);
    return GetExecutionResult(exception_result,
                              SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION);
  }

  return SuccessExecutionResult();
}

/**
 * @brief Handler for the WASI proc_exit function
 *
 * @param info
 */
static void WasiProcExit(const FunctionCallbackInfo<Value>& info) {
  (void)info;
  auto isolate = info.GetIsolate();
  isolate->TerminateExecution();
}

/**
 * @brief Register a function in the object that represents the
 * wasi_snapshot_preview1 module
 *
 * @param isolate
 * @param wasi_snapshot_preview_object
 * @param name
 * @param wasi_function
 */
static void RegisterWasiFunction(Isolate* isolate,
                                 Local<Object>& wasi_snapshot_preview_object,
                                 const std::string& name,
                                 FunctionCallback wasi_function) {
  auto context = isolate->GetCurrentContext();

  auto func_name = TypeConverter<string>::ToV8(isolate, name);
  wasi_snapshot_preview_object
      ->Set(context, func_name,
            v8::FunctionTemplate::New(isolate, wasi_function)
                ->GetFunction(context)
                .ToLocalChecked()
                .As<Object>())
      .Check();
}

/**
 * @brief Generate an object which represents the wasi_snapshot_preview1 module
 *
 * @param isolate
 * @return Local<Object>
 */
static Local<Object> GenerateWasiObject(Isolate* isolate) {
  // Register WASI runtime allowed functions
  auto wasi_snapshot_preview_object = v8::Object::New(isolate);

  RegisterWasiFunction(isolate, wasi_snapshot_preview_object,
                       kWasiProcExitFunctionName, &WasiProcExit);

  return wasi_snapshot_preview_object;
}

/**
 * @brief Register an object in the WASM imports module
 *
 * @param isolate
 * @param imports_object
 * @param name
 * @param new_object
 */
static void RegisterObjectInWasmImports(Isolate* isolate,
                                        Local<Object>& imports_object,
                                        const std::string& name,
                                        Local<Object>& new_object) {
  auto context = isolate->GetCurrentContext();

  auto obj_name = TypeConverter<string>::ToV8(isolate, name);
  imports_object->Set(context, obj_name, new_object).Check();
}

/**
 * @brief Generate an object that represents the WASM imports modules
 *
 * @param isolate
 * @return Local<Object>
 */
static Local<Object> GenerateWasmImports(Isolate* isolate) {
  auto imports_object = v8::Object::New(isolate);

  auto wasi_object = GenerateWasiObject(isolate);

  RegisterObjectInWasmImports(isolate, imports_object, kWasiSnapshotPreview,
                              wasi_object);

  return imports_object;
}

ExecutionResult ExecutionUtils::CompileRunWASM(const RomaString& wasm,
                                               RomaString& err_msg) noexcept {
  auto isolate = Isolate::GetCurrent();
  HandleScope handle_scope(isolate);
  TryCatch try_catch(isolate);
  Local<Context> context(isolate->GetCurrentContext());

  auto module_maybe = WasmModuleObject::Compile(
      isolate,
      MemorySpan<const uint8_t>(
          reinterpret_cast<const unsigned char*>(wasm.c_str()), wasm.length()));
  Local<WasmModuleObject> wasm_module;
  if (!module_maybe.ToLocal(&wasm_module)) {
    ReportException(&try_catch, err_msg);
    return FailureExecutionResult(SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE);
  }

  Local<Value> web_assembly;
  if (!context->Global()
           ->Get(context,
                 String::NewFromUtf8(isolate, kWebAssemblyTag).ToLocalChecked())
           .ToLocal(&web_assembly)) {
    ReportException(&try_catch, err_msg);
    return FailureExecutionResult(
        SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  Local<Value> wasm_instance;
  if (!web_assembly.As<Object>()
           ->Get(context,
                 String::NewFromUtf8(isolate, kInstanceTag).ToLocalChecked())
           .ToLocal(&wasm_instance)) {
    ReportException(&try_catch, err_msg);
    return FailureExecutionResult(
        SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  auto wasm_imports = GenerateWasmImports(isolate);

  Local<Value> instance_args[] = {wasm_module, wasm_imports};
  Local<Value> wasm_construct;
  if (!wasm_instance.As<Object>()
           ->CallAsConstructor(context, 2, instance_args)
           .ToLocal(&wasm_construct)) {
    ReportException(&try_catch, err_msg);
    return FailureExecutionResult(
        SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  Local<Value> wasm_exports;
  if (!wasm_construct.As<Object>()
           ->Get(context,
                 String::NewFromUtf8(isolate, kExportsTag).ToLocalChecked())
           .ToLocal(&wasm_exports)) {
    ReportException(&try_catch, err_msg);
    return FailureExecutionResult(
        SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  // Register wasm_exports object in context.
  if (!context->Global()
           ->Set(context,
                 String::NewFromUtf8(isolate, kRegisteredWasmExports)
                     .ToLocalChecked(),
                 wasm_exports)
           .ToChecked()) {
    ReportException(&try_catch, err_msg);
    return FailureExecutionResult(
        SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  return SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::GetWasmHandler(const RomaString& handler_name,
                                               Local<Value>& handler,
                                               RomaString& err_msg) noexcept {
  auto isolate = Isolate::GetCurrent();
  TryCatch try_catch(isolate);
  Local<Context> context(isolate->GetCurrentContext());

  // Get wasm export object.
  Local<Value> wasm_exports;
  if (!context->Global()
           ->Get(context, String::NewFromUtf8(isolate, kRegisteredWasmExports)
                              .ToLocalChecked())
           .ToLocal(&wasm_exports)) {
    ReportException(&try_catch, err_msg);
    return FailureExecutionResult(
        SC_ROMA_V8_WORKER_WASM_OBJECT_RETRIEVAL_FAILURE);
  }

  // Fetch out the handler name from code object.
  Local<String> local_name;
  if (!ExecutionUtils::RomaStrToLocalStr(handler_name, local_name)) {
    auto exception_result = ReportException(&try_catch, err_msg);
    return GetExecutionResult(exception_result,
                              SC_ROMA_V8_WORKER_BAD_HANDLER_NAME);
  }

  // If there is no handler function, or if it is not a function,
  // bail out
  if (!wasm_exports.As<Object>()->Get(context, local_name).ToLocal(&handler) ||
      !handler->IsFunction()) {
    auto exception_result = ReportException(&try_catch, err_msg);
    return GetExecutionResult(exception_result,
                              SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION);
  }

  return SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::GetExecutionResult(
    const ExecutionResult& exception_result, StatusCode defined_code) noexcept {
  if (exception_result != FailureExecutionResult(SC_UNKNOWN)) {
    return exception_result;
  }

  return FailureExecutionResult(defined_code);
}

Local<Value> ExecutionUtils::ReadFromWasmMemory(Isolate* isolate,
                                                Local<Context>& context,
                                                int32_t offset,
                                                WasmDataType read_value_type) {
  if (offset < 0 || (read_value_type != WasmDataType::kUint32 &&
                     read_value_type != WasmDataType::kString &&
                     read_value_type != WasmDataType::kListOfString)) {
    return Undefined(isolate);
  }

  if (read_value_type == WasmDataType::kUint32) {
    // In this case, the offset is the value so no deserialization is needed.
    return TypeConverter<uint32_t>::ToV8(isolate, offset);
  }

  auto wasm_memory_maybe = GetWasmMemoryObject(isolate, context);
  if (wasm_memory_maybe->IsUndefined()) {
    return Undefined(isolate);
  }

  auto wasm_memory = wasm_memory_maybe.As<WasmMemoryObject>()
                         ->Buffer()
                         ->GetBackingStore()
                         ->Data();
  auto wasm_memory_size = wasm_memory_maybe.As<WasmMemoryObject>()
                              ->Buffer()
                              ->GetBackingStore()
                              ->ByteLength();
  auto wasm_memory_blob = static_cast<uint8_t*>(wasm_memory);

  Local<Value> ret_val = Undefined(isolate);

  if (read_value_type == WasmDataType::kString) {
    string read_str;
    WasmDeserializer::ReadCustomString(wasm_memory_blob, wasm_memory_size,
                                       offset, read_str);

    ret_val = TypeConverter<string>::ToV8(isolate, read_str);
  }
  if (read_value_type == WasmDataType::kListOfString) {
    vector<string> read_vec;
    WasmDeserializer::ReadCustomListOfString(wasm_memory_blob, wasm_memory_size,
                                             offset, read_vec);

    ret_val = TypeConverter<vector<string>>::ToV8(isolate, read_vec);
  }

  return ret_val;
}
}  // namespace google::scp::roma::worker
