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

#include "roma/interface/roma.h"

#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/interface/errors.h"
#include "core/os/src/linux/system_resource_info_provider_linux.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/roma_service/src/roma_service.h"

using absl::OkStatus;
using absl::Status;
using absl::StatusCode;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::os::linux::SystemResourceInfoProviderLinux;
using google::scp::roma::sandbox::roma_service::RomaService;
using std::make_unique;
using std::move;
using std::string;
using std::unique_ptr;
using std::vector;

// This value does not account for runtime memory usage and is only a generic
// estimate based on the memory needed by roma and the steady-state memory
// needed by v8.
static constexpr uint64_t kDefaultMinimumStartupMemoryNeededPerWorkerKb =
    400 * 1024;

namespace google::scp::roma {
namespace {
template <typename RequestT>
Status ExecutionObjectValidation(const std::string& function_name,
                                 const RequestT& invocation_req) {
  if (invocation_req->version_num == 0) {
    return Status(StatusCode::kInvalidArgument,
                  "Roma " + function_name + " failed due to invalid version.");
  }

  if (invocation_req->handler_name.empty()) {
    return Status(
        StatusCode::kInvalidArgument,
        "Roma " + function_name + " failed due to empty handler name.");
  }

  return OkStatus();
}

template <typename RequestT>
Status ExecuteInternal(unique_ptr<RequestT> invocation_req, Callback callback) {
  auto validation = ExecutionObjectValidation("Execute", invocation_req.get());
  if (!validation.ok()) {
    return validation;
  }

  auto* roma_service = RomaService::Instance();
  auto result =
      roma_service->Dispatcher().Dispatch(move(invocation_req), callback);
  if (!result.Successful()) {
    return Status(StatusCode::kInternal,
                  "Roma Execute failed due to: " +
                      std::string(GetErrorMessage(result.status_code)));
  }
  return OkStatus();
}

template <typename RequestT>
Status BatchExecuteInternal(vector<RequestT>& batch,
                            BatchCallback batch_callback) {
  for (auto& request : batch) {
    auto validation = ExecutionObjectValidation("BatchExecute", &request);
    if (!validation.ok()) {
      return validation;
    }
  }

  auto* roma_service = RomaService::Instance();
  auto result = roma_service->Dispatcher().DispatchBatch(batch, batch_callback);
  if (!result.Successful()) {
    return Status(StatusCode::kInternal,
                  "Roma Batch Execute failed due to dispatch error: " +
                      string(GetErrorMessage(result.status_code)));
  }
  return OkStatus();
}

static bool RomaHasEnoughMemoryForStartup(const Config& config) {
  if (!config.enable_startup_memory_check) {
    return true;
  }

  SystemResourceInfoProviderLinux mem_info;
  auto available_memory_or = mem_info.GetAvailableMemoryKb();
  ROMA_VLOG(1) << "Available memory is " << available_memory_or.value()
               << " Kb";
  if (!available_memory_or.result().Successful()) {
    // Failing to read the meminfo file should not stop startup.
    // This mem check is a best-effort check.
    return true;
  }

  if (config.GetStartupMemoryCheckMinimumNeededValueKb) {
    return config.GetStartupMemoryCheckMinimumNeededValueKb() <
           *available_memory_or;
  }

  auto cpu_count = std::thread::hardware_concurrency();
  auto num_processes =
      (config.number_of_workers > 0 && config.number_of_workers <= cpu_count)
          ? config.number_of_workers
          : cpu_count;

  ROMA_VLOG(1) << "Number of workers is " << num_processes;

  auto minimum_memory_needed =
      num_processes * kDefaultMinimumStartupMemoryNeededPerWorkerKb;

  return minimum_memory_needed < *available_memory_or;
}
}  // namespace

Status RomaInit(const Config& config) {
  if (!RomaHasEnoughMemoryForStartup(config)) {
    return Status(StatusCode::kInternal,
                  "Roma startup failed due to insufficient system memory.");
  }

  auto* roma_service = RomaService::Instance(config);
  auto result = roma_service->Init();
  if (!result.Successful()) {
    return Status(StatusCode::kInternal,
                  "Roma initialization failed due to internal error: " +
                      string(GetErrorMessage(result.status_code)));
  }
  result = roma_service->Run();
  if (!result.Successful()) {
    return Status(StatusCode::kInternal,
                  "Roma startup failed due to internal error: " +
                      string(GetErrorMessage(result.status_code)));
  }
  return OkStatus();
}

Status RomaStop() {
  auto* roma_service = RomaService::Instance();
  auto result = roma_service->Stop();
  if (!result.Successful()) {
    return Status(StatusCode::kInternal,
                  "Roma stop failed due to internal error: " +
                      string(GetErrorMessage(result.status_code)));
  }
  RomaService::Delete();
  return OkStatus();
}

Status Execute(unique_ptr<InvocationRequestStrInput> invocation_req,
               Callback callback) {
  return ExecuteInternal(move(invocation_req), callback);
}

Status Execute(unique_ptr<InvocationRequestSharedInput> invocation_req,
               Callback callback) {
  return ExecuteInternal(move(invocation_req), callback);
}

Status BatchExecute(vector<InvocationRequestStrInput>& batch,
                    BatchCallback batch_callback) {
  return BatchExecuteInternal(batch, batch_callback);
}

Status BatchExecute(vector<InvocationRequestSharedInput>& batch,
                    BatchCallback batch_callback) {
  return BatchExecuteInternal(batch, batch_callback);
}

Status LoadCodeObj(unique_ptr<CodeObject> code_object, Callback callback) {
  if (code_object->version_num == 0) {
    return Status(StatusCode::kInternal,
                  "Roma LoadCodeObj failed due to invalid version.");
  }
  if (code_object->js.empty() && code_object->wasm.empty()) {
    return Status(StatusCode::kInternal,
                  "Roma LoadCodeObj failed due to empty code content.");
  }
  if (!code_object->wasm.empty() && !code_object->wasm_bin.empty()) {
    return Status(StatusCode::kInternal,
                  "Roma LoadCodeObj failed due to wasm code and wasm code "
                  "array conflict.");
  }
  if (!code_object->wasm_bin.empty() !=
      code_object->tags.contains(kWasmCodeArrayName)) {
    return Status(StatusCode::kInternal,
                  "Roma LoadCodeObj failed due to empty wasm_bin or "
                  "missing wasm code array name tag.");
  }

  auto* roma_service = RomaService::Instance();
  auto result =
      roma_service->Dispatcher().Broadcast(move(code_object), callback);
  if (!result.Successful()) {
    return Status(StatusCode::kInternal,
                  "Roma LoadCodeObj failed with: " +
                      string(GetErrorMessage(result.status_code)));
  }
  return OkStatus();
}

}  // namespace google::scp::roma
