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

#include "v8_js_engine.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "core/common/time_provider/src/stopwatch.h"
#include "public/core/interface/execution_result.h"
#include "roma/config/src/type_converter.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/constants/constants.h"
#include "roma/sandbox/worker/src/worker_utils.h"
#include "roma/worker/src/execution_utils.h"

#include "error_codes.h"

using absl::flat_hash_map;
using absl::Span;
using absl::string_view;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::Stopwatch;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::
    SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_JSON;
using google::scp::core::errors::
    SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_STRING;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_COULD_NOT_CREATE_ISOLATE;
using google::scp::core::errors::
    SC_ROMA_V8_ENGINE_COULD_NOT_FIND_HANDLER_BY_NAME;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_COULD_NOT_PARSE_SCRIPT_INPUT;
using google::scp::core::errors::
    SC_ROMA_V8_ENGINE_CREATE_COMPILATION_CONTEXT_FAILED_WITH_EMPTY_CODE;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_ERROR_INVOKING_HANDLER;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_EXECUTION_TIMEOUT;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_ISOLATE_NOT_INITIALIZED;
using google::scp::roma::kDefaultExecutionTimeoutMs;
using google::scp::roma::kWasmCodeArrayName;
using google::scp::roma::TypeConverter;
using google::scp::roma::sandbox::constants::kHandlerCallMetricJsEngineNs;
using google::scp::roma::sandbox::constants::kInputParsingMetricJsEngineNs;
using google::scp::roma::sandbox::constants::kJsEngineOneTimeSetupWasmPagesKey;
using google::scp::roma::sandbox::constants::kMaxNumberOfWasm32BitMemPages;
using google::scp::roma::sandbox::constants::kMetadataRomaRequestId;
using google::scp::roma::sandbox::constants::kRequestId;
using google::scp::roma::sandbox::constants::kWasmMemPagesV8PlatformFlag;
using google::scp::roma::sandbox::js_engine::JsEngineExecutionResponse;
using google::scp::roma::sandbox::js_engine::RomaJsEngineCompilationContext;
using google::scp::roma::sandbox::js_engine::v8_js_engine::V8IsolateVisitor;
using google::scp::roma::sandbox::worker::WorkerUtils;
using google::scp::roma::worker::ExecutionUtils;
using google::scp::roma::worker::ExecutionWatchDog;
using std::make_shared;
using std::make_unique;
using std::min;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::stringstream;
using std::to_string;
using std::uint8_t;
using std::vector;
using v8::Array;
using v8::ArrayBuffer;
using v8::Context;
using v8::Function;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::JSON;
using v8::Local;
using v8::MemorySpan;
using v8::ObjectTemplate;
using v8::Script;
using v8::String;
using v8::TryCatch;
using v8::Undefined;
using v8::Value;
using v8::WasmModuleObject;

namespace {
shared_ptr<string> GetCodeFromContext(
    const RomaJsEngineCompilationContext& context) {
  shared_ptr<string> code;

  if (context.has_context) {
    code = static_pointer_cast<string>(context.context);
  }

  return code;
}

/**
 * @brief Create a context in given isolate with isolate_visitors registered.
 *
 * @param isolate
 * @param isolate_visitors
 * @param context
 * @return ExecutionResult
 */
ExecutionResult CreateV8Context(
    v8::Isolate* isolate,
    const std::vector<std::shared_ptr<V8IsolateVisitor>>& isolate_visitors,
    Local<Context>& context) noexcept {
  Local<ObjectTemplate> global_object_template = ObjectTemplate::New(isolate);

  for (auto& visitor : isolate_visitors) {
    auto result = visitor->Visit(isolate, global_object_template);
    RETURN_IF_FAILURE(result);
  }

  context = Context::New(isolate, NULL, global_object_template);
  return SuccessExecutionResult();
}

ExecutionResult GetError(Isolate* isolate, TryCatch& try_catch,
                         uint64_t error_code) noexcept {
  vector<string> errors;

  errors.push_back(GetErrorMessage(error_code));

  if (try_catch.HasCaught()) {
    string error_msg;
    if (!try_catch.Message().IsEmpty() &&
        TypeConverter<string>::FromV8(isolate, try_catch.Message()->Get(),
                                      &error_msg)) {
      errors.push_back(error_msg);
    }
  }

  string error_string;
  for (auto& e : errors) {
    error_string += "\n" + e;
  }
  // Caught error message from V8 sandbox only shows in DEBUG mode.
  DLOG(ERROR) << error_string;

  return FailureExecutionResult(error_code);
}
}  // namespace

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {

ExecutionResult V8JsEngine::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult V8JsEngine::Run() noexcept {
  return execution_watchdog_->Run();
}

ExecutionResult V8JsEngine::Stop() noexcept {
  if (execution_watchdog_) {
    execution_watchdog_->Stop();
  }
  DisposeIsolate();
  return SuccessExecutionResult();
}

ExecutionResult V8JsEngine::OneTimeSetup(
    const flat_hash_map<string, string>& config) noexcept {
  size_t max_wasm_memory_number_of_pages = 0;
  if (config.find(kJsEngineOneTimeSetupWasmPagesKey) != config.end()) {
    auto page_count = config.at(kJsEngineOneTimeSetupWasmPagesKey);
    stringstream page_count_converter;
    page_count_converter << page_count;
    page_count_converter >> max_wasm_memory_number_of_pages;
  }

  pid_t my_pid = getpid();
  string proc_exe_path = string("/proc/") + to_string(my_pid) + "/exe";
  auto my_path = make_unique<char[]>(PATH_MAX);
  readlink(proc_exe_path.c_str(), my_path.get(), PATH_MAX);
  v8::V8::InitializeICUDefaultLocation(my_path.get());
  v8::V8::InitializeExternalStartupData(my_path.get());

  // Set the max number of WASM memory pages
  if (max_wasm_memory_number_of_pages != 0) {
    auto page_count =
        min(max_wasm_memory_number_of_pages, kMaxNumberOfWasm32BitMemPages);
    auto flag_value =
        string(kWasmMemPagesV8PlatformFlag) + to_string(page_count);

    v8::V8::SetFlagsFromString(flag_value.c_str());
  }

  static v8::Platform* v8_platform = [] {
    std::unique_ptr<v8::Platform> v8_platform =
        v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(v8_platform.get());
    v8::V8::Initialize();
    return v8_platform.release();
  }();

  return SuccessExecutionResult();
}

core::ExecutionResult V8JsEngine::CreateSnapshot(v8::StartupData& startup_data,
                                                 const string& js_code,
                                                 string& err_msg) noexcept {
  v8::SnapshotCreator creator(external_references_.data());
  v8::Isolate* isolate = creator.GetIsolate();

  {
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Local<Context> context;
    auto execution_result =
        CreateV8Context(isolate, isolate_visitors_, context);
    RETURN_IF_FAILURE(execution_result);

    Context::Scope context_scope(context);
    //  Compile and run JavaScript code object.
    execution_result = ExecutionUtils::CompileRunJS(js_code, err_msg);
    RETURN_IF_FAILURE(execution_result);

    // Set above context with compiled and run code as the default context for
    // the StartupData blob to create.
    creator.SetDefaultContext(context);
  }
  startup_data =
      creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  return core::SuccessExecutionResult();
}

core::ExecutionResult V8JsEngine::CreateSanpshotWithGlobals(
    v8::StartupData& startup_data, const Span<const uint8_t>& wasm,
    const flat_hash_map<string, string>& metadata, string& err_msg) noexcept {
  v8::SnapshotCreator creator(external_references_.data());
  v8::Isolate* isolate = creator.GetIsolate();

  {
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Local<Context> context;
    auto execution_result =
        CreateV8Context(isolate, isolate_visitors_, context);
    RETURN_IF_FAILURE(execution_result);

    Context::Scope context_scope(context);
    auto wasm_code_array_name_or =
        WorkerUtils::GetValueFromMetadata(metadata, kWasmCodeArrayName);
    if (!wasm_code_array_name_or.result().Successful()) {
      LOG(ERROR) << string("Get wasm code array name from metadata with error ")
                 << GetErrorMessage(
                        wasm_code_array_name_or.result().status_code);
      return wasm_code_array_name_or.result();
    }

    v8::Local<v8::String> name;
    name = TypeConverter<string>::ToV8(isolate, wasm_code_array_name_or.value())
               .As<String>();

    context->Global()->Set(
        context, name,
        TypeConverter<uint8_t*>::ToV8(isolate, wasm.data(), wasm.size()));
    // Set above context with compiled and run code as the default context for
    // the StartupData blob to create.
    creator.SetDefaultContext(context);
  }
  startup_data =
      creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  return core::SuccessExecutionResult();
}

static size_t NearHeapLimitCallback(void* data, size_t current_heap_limit,
                                    size_t initial_heap_limit) {
  LOG(ERROR) << "OOM in JS execution, exiting...";
  return 0;
}

ExecutionResultOr<v8::Isolate*> V8JsEngine::CreateIsolate(
    const v8::StartupData& startup_data) noexcept {
  Isolate::CreateParams params;

  // Configure v8 resource constraints if initial_heap_size_in_mb or
  // maximum_heap_size_in_mb is nonzero.
  if (v8_resource_constraints_.initial_heap_size_in_mb > 0 ||
      v8_resource_constraints_.maximum_heap_size_in_mb > 0) {
    params.constraints.ConfigureDefaultsFromHeapSize(
        v8_resource_constraints_.initial_heap_size_in_mb * kMB,
        v8_resource_constraints_.maximum_heap_size_in_mb * kMB);
  }

  params.external_references = external_references_.data();
  params.array_buffer_allocator = ArrayBuffer::Allocator::NewDefaultAllocator();

  // Configure create_params with startup_data if startup_data is
  // available.
  if (startup_data.raw_size > 0 && startup_data.data != nullptr) {
    params.snapshot_blob = &startup_data;
  }

  auto isolate = Isolate::New(params);

  if (!isolate) {
    return FailureExecutionResult(SC_ROMA_V8_ENGINE_COULD_NOT_CREATE_ISOLATE);
  }

  isolate->AddNearHeapLimitCallback(NearHeapLimitCallback, NULL);

  return isolate;
}

void V8JsEngine::DisposeIsolate() noexcept {
  if (v8_isolate_) {
    v8_isolate_->Dispose();
    v8_isolate_ = nullptr;
  }
}

void V8JsEngine::StartWatchdogTimer(
    v8::Isolate* isolate,
    const flat_hash_map<string, string>& metadata) noexcept {
  // Get the timeout value from metadata. If no timeout tag is set, the
  // default value kDefaultExecutionTimeoutMs will be used.
  int timeout_ms = kDefaultExecutionTimeoutMs;
  auto timeout_str_or =
      WorkerUtils::GetValueFromMetadata(metadata, kTimeoutMsTag);
  if (timeout_str_or.result().Successful()) {
    auto timeout_int_or = WorkerUtils::ConvertStrToInt(timeout_str_or.value());
    if (timeout_int_or.result().Successful()) {
      timeout_ms = timeout_int_or.value();
    } else {
      LOG(ERROR) << "Timeout tag parsing with error "
                 << GetErrorMessage(timeout_int_or.result().status_code);
    }
  }
  ROMA_VLOG(1) << "StartWatchdogTimer timeout set to " << timeout_ms << " ms";
  execution_watchdog_->StartTimer(isolate, timeout_ms);
}

void V8JsEngine::StopWatchdogTimer() noexcept {
  execution_watchdog_->EndTimer();
}

ExecutionResultOr<RomaJsEngineCompilationContext>
V8JsEngine::CreateCompilationContext(
    const string& code, const Span<const uint8_t>& wasm,
    const flat_hash_map<string, string>& metadata, string& err_msg) noexcept {
  if (code.empty()) {
    return FailureExecutionResult(
        SC_ROMA_V8_ENGINE_CREATE_COMPILATION_CONTEXT_FAILED_WITH_EMPTY_CODE);
  }

  RomaJsEngineCompilationContext out_context;
  auto snapshot_context = make_shared<SnapshotCompilationContext>();
  // If wasm code array exists, a snapshot with global wasm code array will be
  // created. Otherwise, a normal snapshot containing compiled JS code will be
  // created.
  bool js_with_wasm = !wasm.empty();
  auto execution_result =
      js_with_wasm
          ? CreateSanpshotWithGlobals(snapshot_context->startup_data, wasm,
                                      metadata, err_msg)
          : CreateSnapshot(snapshot_context->startup_data, code, err_msg);
  ExecutionResultOr<v8::Isolate*> isolate_or;

  if (execution_result.Successful()) {
    isolate_or = CreateIsolate(snapshot_context->startup_data);
    RETURN_IF_FAILURE(isolate_or.result());
    snapshot_context->cache_type = CacheType::kSnapshot;

    if (js_with_wasm) {
      auto wasm_compile_result =
          CompileWasmCodeArray(isolate_or.value(), wasm, err_msg);
      if (!wasm_compile_result.Successful()) {
        LOG(ERROR) << "Compile wasm module failed with "
                   << GetErrorMessage(wasm_compile_result.status_code);
        DLOG(ERROR) << "Compile wasm module failed with debug error" << err_msg;
        isolate_or.value()->Dispose();
        return wasm_compile_result;
      }
      execution_result = ExecutionUtils::CreateUnboundScript(
          snapshot_context->unbound_script, isolate_or.value(), code, err_msg);
      if (!execution_result.Successful()) {
        LOG(ERROR) << "CreateUnboundScript failed with "
                   << GetErrorMessage(execution_result.status_code);
        DLOG(ERROR) << "CreateUnboundScript failed with debug errors "
                    << err_msg;
        isolate_or.value()->Dispose();
        return execution_result;
      }
      snapshot_context->cache_type = CacheType::kUnboundScript;
    }

    ROMA_VLOG(2) << "compilation context cache type is V8 snapshot";
  } else {
    LOG(ERROR) << "CreateSnapshot failed with "
               << GetErrorMessage(execution_result.status_code);
    // err_msg may contain confidential message which only shows in DEBUG mode.
    DLOG(ERROR) << "CreateSnapshot failed with debug errors " << err_msg;
    // Return the failure if it isn't caused by global WebAssembly.
    if (!ExecutionUtils::CheckErrorWithWebAssembly(err_msg)) {
      return execution_result;
    }

    isolate_or = CreateIsolate();
    RETURN_IF_FAILURE(isolate_or.result());

    // TODO(b/298062607): deprecate err_msg, all exceptions should being caught
    // by GetError().
    execution_result = ExecutionUtils::CreateUnboundScript(
        snapshot_context->unbound_script, isolate_or.value(), code, err_msg);
    if (!execution_result.Successful()) {
      LOG(ERROR) << "CreateUnboundScript failed with "
                 << GetErrorMessage(execution_result.status_code);
      // err_msg may contain confidential message which only shows in DEBUG
      // mode.
      DLOG(ERROR) << "CreateUnboundScript failed with debug errors " << err_msg;
      // Dispose the isolate as it won't being used.
      isolate_or.value()->Dispose();
      return execution_result;
    }

    snapshot_context->cache_type = CacheType::kUnboundScript;
    ROMA_VLOG(2) << "compilation context cache type is V8 UnboundScript";
  }

  // Snapshot the isolate with compilation context and also initialize a
  // execution watchdog inside the isolate.
  snapshot_context->v8_isolate = *isolate_or;

  out_context.has_context = true;
  out_context.context = snapshot_context;
  return out_context;
}

core::ExecutionResult V8JsEngine::CompileWasmCodeArray(
    Isolate* isolate, const Span<const uint8_t>& wasm,
    string& err_msg) noexcept {
  Isolate::Scope isolate_scope(isolate);
  // Create a handle scope to keep the temporary object references.
  HandleScope handle_scope(isolate);
  // Set up an exception handler before calling the Process function
  TryCatch try_catch(isolate);

  Local<Context> v8_context = Context::New(isolate);
  Context::Scope context_scope(v8_context);

  // Check whether wasm module can compile
  auto module_maybe = v8::WasmModuleObject::Compile(
      isolate,
      v8::MemorySpan<const uint8_t>(
          reinterpret_cast<const unsigned char*>(wasm.data()), wasm.size()));
  if (module_maybe.IsEmpty()) {
    return FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE);
  }
  return SuccessExecutionResult();
}

core::ExecutionResultOr<ExecutionResponse> V8JsEngine::ExecuteJs(
    const shared_ptr<SnapshotCompilationContext>& current_compilation_context,
    const string& function_name, const vector<string_view>& input,
    const flat_hash_map<string, string>& metadata) noexcept {
  ExecutionResponse execution_response;
  Stopwatch stopwatch;

  auto v8_isolate = current_compilation_context->v8_isolate;
  Isolate::Scope isolate_scope(v8_isolate);
  // Create a handle scope to keep the temporary object references.
  HandleScope handle_scope(v8_isolate);
  // Set up an exception handler before calling the Process function
  TryCatch try_catch(v8_isolate);

  Local<Context> v8_context = Context::New(v8_isolate);
  Context::Scope context_scope(v8_context);

  string err_msg;
  // Binding UnboundScript to current context when the compilation context is
  // kUnboundScript.
  if (current_compilation_context->cache_type == CacheType::kUnboundScript) {
    auto result = ExecutionUtils::BindUnboundScript(
        current_compilation_context->unbound_script, err_msg);
    if (!result.Successful()) {
      LOG(ERROR) << "BindUnboundScript failed with "
                 << GetErrorMessage(result.status_code);
      DLOG(ERROR) << "BindUnboundScript failed with debug errors " << err_msg;
      return result;
    }
  }

  Local<Value> handler;
  auto result = ExecutionUtils::GetJsHandler(function_name, handler, err_msg);
  if (!result.Successful()) {
    LOG(ERROR) << "GetJsHandler failed with "
               << GetErrorMessage(result.status_code);
    DLOG(ERROR) << "GetJsHandler failed with debug errors " << err_msg;
    return result;
  }

  {
    Local<Function> handler_func = handler.As<Function>();

    stopwatch.Start();
    auto argc = input.size();
    Local<Array> argv_array = ExecutionUtils::ParseAsJsInput(input);
    // If argv_array size doesn't match with input. Input conversion failed.
    if (argv_array.IsEmpty() || argv_array->Length() != argc) {
      LOG(ERROR) << "Could not parse the inputs";
      return GetError(v8_isolate, try_catch,
                      SC_ROMA_V8_ENGINE_COULD_NOT_PARSE_SCRIPT_INPUT);
    }
    Local<Value> argv[argc];
    for (size_t i = 0; i < argc; ++i) {
      argv[i] = argv_array->Get(v8_context, i).ToLocalChecked();
    }
    auto input_parse_elapsed_ns = stopwatch.Stop();
    execution_response.metrics[kInputParsingMetricJsEngineNs] =
        input_parse_elapsed_ns.count();

    // Set the request ID in the global object
    auto request_id_label =
        TypeConverter<string>::ToV8(v8_isolate, kMetadataRomaRequestId)
            .As<String>();
    auto request_id_or =
        WorkerUtils::GetValueFromMetadata(metadata, kRequestId);
    if (request_id_or.Successful()) {
      auto request_id =
          TypeConverter<string>::ToV8(v8_isolate, *request_id_or).As<String>();
      v8_context->Global()
          ->Set(v8_context, request_id_label, request_id)
          .Check();
    } else {
      LOG(ERROR) << "Could not read request ID from metadata.";
    }

    stopwatch.Start();
    Local<Value> result;
    if (!handler_func->Call(v8_context, v8_context->Global(), argc, argv)
             .ToLocal(&result)) {
      LOG(ERROR) << "Handler function calling failed";
      return GetError(v8_isolate, try_catch,
                      SC_ROMA_V8_ENGINE_ERROR_INVOKING_HANDLER);
    }

    if (result->IsPromise()) {
      string error_msg;
      auto execution_result =
          ExecutionUtils::V8PromiseHandler(v8_isolate, result, error_msg);
      if (!execution_result.Successful()) {
        DLOG(ERROR) << "V8 Promise execution failed" << error_msg;
        return GetError(v8_isolate, try_catch, execution_result.status_code);
      }
    }
    auto handler_call_elapsed_ns = stopwatch.Stop();
    execution_response.metrics[kHandlerCallMetricJsEngineNs] =
        handler_call_elapsed_ns.count();

    auto result_json_maybe = JSON::Stringify(v8_context, result);
    Local<String> result_json;
    if (!result_json_maybe.ToLocal(&result_json)) {
      LOG(ERROR) << "Failed to convert the V8 JSON result to Local string";
      return GetError(v8_isolate, try_catch,
                      SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_JSON);
    }

    auto conversion_worked = TypeConverter<string>::FromV8(
        v8_isolate, result_json, execution_response.response.get());
    if (!conversion_worked) {
      LOG(ERROR) << "Failed to convert the V8 Local string to std::string";
      return GetError(v8_isolate, try_catch,
                      SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_STRING);
    }
  }

  return execution_response;
}

ExecutionResultOr<JsEngineExecutionResponse> V8JsEngine::CompileAndRunJs(
    const string& code, const string& function_name,
    const vector<string_view>& input,
    const flat_hash_map<string, string>& metadata,
    const RomaJsEngineCompilationContext& context) noexcept {
  string err_msg;
  JsEngineExecutionResponse execution_response;
  shared_ptr<SnapshotCompilationContext> current_compilation_context;
  return CompileAndRunJsWithWasm(code, Span<const uint8_t>(), function_name,
                                 input, metadata, context);
}

ExecutionResultOr<JsEngineExecutionResponse> V8JsEngine::CompileAndRunWasm(
    const string& code, const string& function_name,
    const vector<string_view>& input,
    const flat_hash_map<string, string>& metadata,
    const RomaJsEngineCompilationContext& context) noexcept {
  JsEngineExecutionResponse execution_response;

  // temp solution for CompileAndRunWasm(). This will update in next PR soon.
  auto isolate_or = CreateIsolate();
  RETURN_IF_FAILURE(isolate_or.result());
  v8_isolate_ = *isolate_or;

  if (!v8_isolate_) {
    return FailureExecutionResult(SC_ROMA_V8_ENGINE_ISOLATE_NOT_INITIALIZED);
  }

  // Start execution watchdog to timeout the execution if it runs too long.
  StartWatchdogTimer(v8_isolate_, metadata);

  string input_code;
  RomaJsEngineCompilationContext out_context;
  // For now we just store and reuse the actual code as context.
  auto context_code = GetCodeFromContext(context);
  if (context_code) {
    input_code = *context_code;
    out_context = context;
  } else {
    input_code = code;
    out_context.has_context = true;
    out_context.context = make_shared<string>(code);
  }
  execution_response.compilation_context = out_context;

  auto isolate = v8_isolate_;
  vector<string> errors;
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  Local<Context> v8_context;

  {
    auto execution_result =
        CreateV8Context(isolate, isolate_visitors_, v8_context);
    RETURN_IF_FAILURE(execution_result);

    Context::Scope context_scope(v8_context);
    Local<Context> context(isolate->GetCurrentContext());
    TryCatch try_catch(isolate);

    std::string errors;
    auto result = ExecutionUtils::CompileRunWASM(input_code, errors);
    if (!result.Successful()) {
      LOG(ERROR) << errors;
      return GetError(v8_isolate_, try_catch, result.status_code);
    }

    if (!function_name.empty()) {
      Local<Value> wasm_handler;
      result =
          ExecutionUtils::GetWasmHandler(function_name, wasm_handler, errors);
      if (!result.Successful()) {
        LOG(ERROR) << errors;
        return GetError(v8_isolate_, try_catch, result.status_code);
      }

      auto wasm_input_array =
          ExecutionUtils::ParseAsWasmInput(v8_isolate_, context, input);

      if (wasm_input_array.IsEmpty() ||
          wasm_input_array->Length() != input.size()) {
        return GetError(isolate, try_catch,
                        SC_ROMA_V8_ENGINE_COULD_NOT_PARSE_SCRIPT_INPUT);
      }

      auto input_length = wasm_input_array->Length();
      Local<Value> wasm_input[input_length];
      for (size_t i = 0; i < input_length; ++i) {
        wasm_input[i] = wasm_input_array->Get(context, i).ToLocalChecked();
      }

      auto handler_function = wasm_handler.As<Function>();

      Local<Value> wasm_result;
      if (!handler_function
               ->Call(context, context->Global(), input_length, wasm_input)
               .ToLocal(&wasm_result)) {
        return GetError(v8_isolate_, try_catch,
                        SC_ROMA_V8_ENGINE_ERROR_INVOKING_HANDLER);
      }

      auto offset = wasm_result.As<Int32>()->Value();
      auto wasm_execution_output = ExecutionUtils::ReadFromWasmMemory(
          v8_isolate_, context, offset, WasmDataType::kString);
      auto result_json_maybe = JSON::Stringify(context, wasm_execution_output);
      Local<String> result_json;
      if (!result_json_maybe.ToLocal(&result_json)) {
        return GetError(v8_isolate_, try_catch,
                        SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_STRING);
      }

      auto conversion_worked = TypeConverter<string>::FromV8(
          isolate, result_json,
          execution_response.execution_response.response.get());
      if (!conversion_worked) {
        return GetError(isolate, try_catch,
                        SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_STRING);
      }
    }
  }

  // End execution_watchdog_ in case it terminate the standby isolate.
  StopWatchdogTimer();

  return execution_response;
}

ExecutionResultOr<JsEngineExecutionResponse>
V8JsEngine::CompileAndRunJsWithWasm(
    const string& code, const Span<const uint8_t>& wasm,
    const string& function_name, const vector<string_view>& input,
    const flat_hash_map<string, string>& metadata,
    const RomaJsEngineCompilationContext& context) noexcept {
  string err_msg;
  JsEngineExecutionResponse execution_response;
  shared_ptr<SnapshotCompilationContext> current_compilation_context;
  if (!context.has_context) {
    auto context_or = CreateCompilationContext(code, wasm, metadata, err_msg);
    if (!context_or.result().Successful()) {
      LOG(ERROR) << string("CreateCompilationContext failed with ") << err_msg;
      return context_or.result();
    }

    execution_response.compilation_context = context_or.value();
    current_compilation_context =
        std::static_pointer_cast<SnapshotCompilationContext>(
            context_or.value().context);
  } else {
    current_compilation_context =
        std::static_pointer_cast<SnapshotCompilationContext>(context.context);
  }

  auto v8_isolate = current_compilation_context->v8_isolate;

  if (!v8_isolate) {
    return FailureExecutionResult(SC_ROMA_V8_ENGINE_ISOLATE_NOT_INITIALIZED);
  }

  // No function_name just return execution_response which may contain
  // RomaJsEngineCompilationContext.
  if (function_name.empty()) {
    return execution_response;
  }

  StartWatchdogTimer(v8_isolate, metadata);

  auto execution_result =
      ExecuteJs(current_compilation_context, function_name, input, metadata);

  // End execution_watchdog_ in case it terminate the standby isolate.
  StopWatchdogTimer();

  if (execution_result.Successful()) {
    execution_response.execution_response = execution_result.value();
    return execution_response;
  }

  // Return timeout error if the watchdog called isolate terminate.
  if (execution_watchdog_->IsTerminateCalled()) {
    return FailureExecutionResult(SC_ROMA_V8_ENGINE_EXECUTION_TIMEOUT);
  }
  return execution_result.result();
}
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine
