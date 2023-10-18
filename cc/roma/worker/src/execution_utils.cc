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
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ROMA_V8_WORKER_BAD_HANDLER_NAME;
using google::scp::core::errors::SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_SCRIPT_RUN_FAILURE;
using google::scp::roma::wasm::RomaWasmListOfStringRepresentation;
using google::scp::roma::wasm::RomaWasmStringRepresentation;
using google::scp::roma::wasm::WasmDeserializer;
using google::scp::roma::wasm::WasmSerializer;
using std::shared_ptr;
using std::string;
using std::vector;

using v8::Array;
using v8::Context;
using v8::External;
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
static constexpr char kWasmMemory[] = "memory";
static constexpr char kWasiSnapshotPreview[] = "wasi_snapshot_preview1";
static constexpr char kWasiProcExitFunctionName[] = "proc_exit";

ExecutionResult ExecutionUtils::CompileRunJS(
    const std::string& js, std::string& err_msg,
    Local<UnboundScript>* unbound_script) noexcept {
  auto isolate = Isolate::GetCurrent();
  TryCatch try_catch(isolate);
  Local<Context> context(isolate->GetCurrentContext());

  Local<v8::String> js_source =
      v8::String::NewFromUtf8(isolate, js.data(), v8::NewStringType::kNormal,
                              static_cast<uint32_t>(js.length()))
          .ToLocalChecked();
  Local<v8::Script> script;
  if (!v8::Script::Compile(context, js_source).ToLocal(&script)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE);
  }

  if (unbound_script) {
    *unbound_script = script->GetUnboundScript();
  }

  Local<Value> script_result;
  if (!script->Run(context).ToLocal(&script_result)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_SCRIPT_RUN_FAILURE);
  }

  return core::SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::GetJsHandler(const std::string& handler_name,
                                             Local<Value>& handler,
                                             std::string& err_msg) noexcept {
  if (handler_name.empty()) {
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_BAD_HANDLER_NAME);
  }
  auto isolate = Isolate::GetCurrent();
  TryCatch try_catch(isolate);
  Local<Context> context(isolate->GetCurrentContext());

  Local<v8::String> local_name =
      v8::String::NewFromUtf8(isolate, handler_name.data(),
                              v8::NewStringType::kNormal,
                              static_cast<uint32_t>(handler_name.length()))
          .ToLocalChecked();

  // If there is no handler function, or if it is not a function,
  // bail out
  if (!context->Global()->Get(context, local_name).ToLocal(&handler) ||
      !handler->IsFunction()) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION);
  }

  return core::SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::CompileRunWASM(const std::string& wasm,
                                               std::string& err_msg) noexcept {
  auto isolate = Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  TryCatch try_catch(isolate);
  Local<Context> context(isolate->GetCurrentContext());

  auto module_maybe = v8::WasmModuleObject::Compile(
      isolate,
      v8::MemorySpan<const uint8_t>(
          reinterpret_cast<const unsigned char*>(wasm.c_str()), wasm.length()));
  Local<v8::WasmModuleObject> wasm_module;
  if (!module_maybe.ToLocal(&wasm_module)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE);
  }

  Local<Value> web_assembly;
  if (!context->Global()
           ->Get(context, v8::String::NewFromUtf8(isolate, kWebAssemblyTag)
                              .ToLocalChecked())
           .ToLocal(&web_assembly)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  Local<Value> wasm_instance;
  if (!web_assembly.As<v8::Object>()
           ->Get(
               context,
               v8::String::NewFromUtf8(isolate, kInstanceTag).ToLocalChecked())
           .ToLocal(&wasm_instance)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  auto wasm_imports = ExecutionUtils::GenerateWasmImports(isolate);

  Local<Value> instance_args[] = {wasm_module, wasm_imports};
  Local<Value> wasm_construct;
  if (!wasm_instance.As<v8::Object>()
           ->CallAsConstructor(context, 2, instance_args)
           .ToLocal(&wasm_construct)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  Local<Value> wasm_exports;
  if (!wasm_construct.As<v8::Object>()
           ->Get(context,
                 v8::String::NewFromUtf8(isolate, kExportsTag).ToLocalChecked())
           .ToLocal(&wasm_exports)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  // Register wasm_exports object in context.
  if (!context->Global()
           ->Set(context,
                 v8::String::NewFromUtf8(isolate, kRegisteredWasmExports)
                     .ToLocalChecked(),
                 wasm_exports)
           .ToChecked()) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  return core::SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::GetWasmHandler(const std::string& handler_name,
                                               Local<Value>& handler,
                                               std::string& err_msg) noexcept {
  auto isolate = Isolate::GetCurrent();
  TryCatch try_catch(isolate);
  Local<Context> context(isolate->GetCurrentContext());

  // Get wasm export object.
  Local<Value> wasm_exports;
  if (!context->Global()
           ->Get(context,
                 v8::String::NewFromUtf8(isolate, kRegisteredWasmExports)
                     .ToLocalChecked())
           .ToLocal(&wasm_exports)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_RETRIEVAL_FAILURE);
  }

  // Fetch out the handler name from code object.
  auto str = std::string(handler_name.c_str(), handler_name.size());
  Local<v8::String> local_name =
      TypeConverter<std::string>::ToV8(isolate, str).As<v8::String>();

  // If there is no handler function, or if it is not a function,
  // bail out
  if (!wasm_exports.As<v8::Object>()
           ->Get(context, local_name)
           .ToLocal(&handler) ||
      !handler->IsFunction()) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION);
  }

  return core::SuccessExecutionResult();
}

void ExecutionUtils::GlobalV8FunctionCallback(
    const FunctionCallbackInfo<Value>& info) {
  auto isolate = info.GetIsolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);

  if (info.Data().IsEmpty() || !info.Data()->IsExternal()) {
    isolate->ThrowError("Unexpected data in global callback");
    return;
  }

  // Get the user-provided function
  Local<External> data_object = Local<External>::Cast(info.Data());
  auto user_function =
      reinterpret_cast<FunctionBindingObjectBase*>(data_object->Value());

  if (user_function->Signature != FunctionBindingObjectBase::KnownSignature) {
    // This signals a bad cast. The pointer we got is not really a
    // FunctionBindingObjectBase
    isolate->ThrowError("Unexpected function in global callback");
    return;
  }

  user_function->InvokeInternalHandler(info);
}

ExecutionResult ExecutionUtils::CreateUnboundScript(
    Global<UnboundScript>& unbound_script, Isolate* isolate,
    const std::string& js, std::string& err_msg) noexcept {
  Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<UnboundScript> local_unbound_script;
  auto execution_result =
      ExecutionUtils::CompileRunJS(js, err_msg, &local_unbound_script);
  RETURN_IF_FAILURE(execution_result);

  // Store unbound_script_ in a Global handle in isolate.
  unbound_script.Reset(isolate, local_unbound_script);

  return core::SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::BindUnboundScript(
    const Global<UnboundScript>& global_unbound_script,
    std::string& err_msg) noexcept {
  auto isolate = Isolate::GetCurrent();
  TryCatch try_catch(isolate);
  Local<Context> context(isolate->GetCurrentContext());
  Context::Scope context_scope(context);

  Local<UnboundScript> unbound_script =
      Local<UnboundScript>::New(isolate, global_unbound_script);

  Local<Value> script_result;
  if (!unbound_script->BindToCurrentContext()->Run(context).ToLocal(
          &script_result)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_BIND_UNBOUND_SCRIPT_FAILED);
  }

  return core::SuccessExecutionResult();
}

Local<Value> ExecutionUtils::GetWasmMemoryObject(Isolate* isolate,
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

Local<Array> ExecutionUtils::InputToLocalArgv(
    const std::vector<std::string_view>& input, bool is_wasm) noexcept {
  auto isolate = Isolate::GetCurrent();
  auto context = isolate->GetCurrentContext();

  if (is_wasm) {
    return ExecutionUtils::ParseAsWasmInput(isolate, context, input);
  }

  return ExecutionUtils::ParseAsJsInput(input);
}

string ExecutionUtils::ExtractMessage(Isolate* isolate,
                                      Local<v8::Message> message) noexcept {
  string exception_msg;
  TypeConverter<string>::FromV8(isolate, message->Get(), &exception_msg);
  // We want to return a message of the form:
  //
  //     line 7: Uncaught ReferenceError: blah is not defined.
  //
  int line;
  // Sometimes for multi-line errors there is no line number.
  if (!message->GetLineNumber(isolate->GetCurrentContext()).To(&line)) {
    return absl::StrFormat("%s", exception_msg);
  }

  return absl::StrFormat("line %i: %s", line, exception_msg);
}

Local<v8::Array> ExecutionUtils::ParseAsJsInput(
    const std::vector<std::string_view>& input) {
  auto isolate = Isolate::GetCurrent();
  auto context = isolate->GetCurrentContext();

  const int argc = input.size();

  Local<v8::Array> argv = v8::Array::New(isolate, argc);
  for (auto i = 0; i < argc; ++i) {
    Local<v8::String> arg_str =
        v8::String::NewFromUtf8(isolate, input[i].data(),
                                v8::NewStringType::kNormal,
                                static_cast<uint32_t>(input[i].length()))
            .ToLocalChecked();

    Local<Value> arg = v8::Undefined(isolate);
    if (arg_str->Length() > 0 &&
        !v8::JSON::Parse(context, arg_str).ToLocal(&arg)) {
      return Local<v8::Array>();
    }
    if (!argv->Set(context, i, arg).ToChecked()) {
      return Local<v8::Array>();
    }
  }

  return argv;
}

Local<v8::Array> ExecutionUtils::ParseAsWasmInput(
    Isolate* isolate, Local<Context>& context,
    const std::vector<std::string_view>& input) {
  // Parse it into JS types so we can distinguish types
  auto parsed_args = ExecutionUtils::ParseAsJsInput(input);
  const int argc = parsed_args.IsEmpty() ? 0 : parsed_args->Length();

  // Parsing the input failed
  if (argc != input.size()) {
    return Local<v8::Array>();
  }

  Local<v8::Array> argv = v8::Array::New(isolate, argc);

  auto wasm_memory = ExecutionUtils::GetWasmMemoryObject(isolate, context);

  if (wasm_memory->IsUndefined()) {
    // The module has no memory object. This is either a very basic WASM, or
    // invalid, we'll just exit early, and pass the input as it was parsed.
    return parsed_args;
  }

  auto wasm_memory_blob = wasm_memory.As<v8::WasmMemoryObject>()
                              ->Buffer()
                              ->GetBackingStore()
                              ->Data();
  auto wasm_memory_size = wasm_memory.As<v8::WasmMemoryObject>()
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
      return Local<v8::Array>();
    }

    Local<Value> new_arg;

    if (arg->IsUint32() || arg->IsInt32()) {
      // No serialization needed
      new_arg = arg;
    }
    if (arg->IsString()) {
      std::string str_value;
      TypeConverter<std::string>::FromV8(isolate, arg, &str_value);
      auto string_ptr_in_wasm_memory = wasm::WasmSerializer::WriteCustomString(
          wasm_memory_blob, wasm_memory_size, wasm_memory_offset, str_value);

      // The serialization failed
      if (string_ptr_in_wasm_memory == UINT32_MAX) {
        return Local<v8::Array>();
      }

      new_arg =
          TypeConverter<uint32_t>::ToV8(isolate, string_ptr_in_wasm_memory);
      wasm_memory_offset +=
          wasm::RomaWasmStringRepresentation::ComputeMemorySizeFor(str_value);
    }
    if (arg->IsArray()) {
      std::vector<std::string> vec_value;
      bool worked = TypeConverter<std::vector<std::string>>::FromV8(
          isolate, arg, &vec_value);

      if (!worked) {
        // This means the array is not an array of string
        return Local<v8::Array>();
      }

      auto list_ptr_in_wasm_memory =
          wasm::WasmSerializer::WriteCustomListOfString(
              wasm_memory_blob, wasm_memory_size, wasm_memory_offset,
              vec_value);

      // The serialization failed
      if (list_ptr_in_wasm_memory == UINT32_MAX) {
        return Local<v8::Array>();
      }

      new_arg = TypeConverter<uint32_t>::ToV8(isolate, list_ptr_in_wasm_memory);
      wasm_memory_offset +=
          wasm::RomaWasmListOfStringRepresentation::ComputeMemorySizeFor(
              vec_value);
    }

    if (!argv->Set(context, i, new_arg).ToChecked()) {
      return Local<v8::Array>();
    }
  }

  return argv;
}

string ExecutionUtils::DescribeError(Isolate* isolate,
                                     TryCatch* try_catch) noexcept {
  const Local<Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    return std::string();
  }

  return ExecutionUtils::ExtractMessage(isolate, message);
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
Local<Object> ExecutionUtils::GenerateWasmImports(Isolate* isolate) {
  auto imports_object = v8::Object::New(isolate);

  auto wasi_object = GenerateWasiObject(isolate);

  RegisterObjectInWasmImports(isolate, imports_object, kWasiSnapshotPreview,
                              wasi_object);

  return imports_object;
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

ExecutionResult ExecutionUtils::V8PromiseHandler(Isolate* isolate,
                                                 Local<Value>& result,
                                                 std::string& err_msg) {
  // We don't need a callback handler for now. The default handler will wrap
  // the successful result of Promise::kFulfilled and the exception message of
  // Promise::kRejected.
  auto promise = result.As<v8::Promise>();

  // Wait until promise state isn't pending.
  while (promise->State() == v8::Promise::kPending) {
    isolate->PerformMicrotaskCheckpoint();
  }

  if (promise->State() == v8::Promise::kRejected) {
    // Extract the exception message from a rejected promise.
    const Local<v8::Message> message =
        v8::Exception::CreateMessage(isolate, promise->Result());
    err_msg = ExecutionUtils::ExtractMessage(isolate, message);
    promise->MarkAsHandled();
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_ASYNC_EXECUTION_FAILED);
  }

  result = promise->Result();
  return core::SuccessExecutionResult();
}

}  // namespace google::scp::roma::worker
