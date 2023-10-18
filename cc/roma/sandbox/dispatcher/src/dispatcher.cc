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

#include "dispatcher.h"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/constants/constants.h"

using absl::StatusOr;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::unique_ptr;
using std::vector;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace google::scp::roma::sandbox::dispatcher {
ExecutionResult Dispatcher::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult Dispatcher::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult Dispatcher::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult Dispatcher::Broadcast(unique_ptr<CodeObject> code_object,
                                      Callback broadcast_callback) noexcept {
  auto worker_count = worker_pool_->GetPoolSize();
  auto finished_counter = make_shared<atomic<size_t>>(0);
  auto responses_storage =
      make_shared<vector<unique_ptr<StatusOr<ResponseObject>>>>(worker_count);

  for (size_t worker_index = 0; worker_index < worker_count; worker_index++) {
    auto callback =
        [worker_count, responses_storage, finished_counter, broadcast_callback,
         worker_index](unique_ptr<StatusOr<ResponseObject>> response) {
          auto& all_resp = *responses_storage;
          // Store responses in the vector
          all_resp[worker_index].swap(response);
          auto finished_value = finished_counter->fetch_add(1);
          // Go through the responses and call the callback on the first failed
          // one. If all succeeded, call the first callback.
          if (finished_value + 1 == worker_count) {
            for (auto& resp : all_resp) {
              if (!resp->ok()) {
                broadcast_callback(::std::move(resp));
                return;
              }
            }
            broadcast_callback(::std::move(all_resp[0]));
          }
        };

    auto code_object_copy = make_unique<CodeObject>(*code_object);

    auto dispatch_result =
        InternalDispatch(move(code_object_copy), callback, worker_index);

    if (!dispatch_result.Successful()) {
      LOG(ERROR) << "Broadcast failed at the " << worker_index
                 << " worker with error "
                 << GetErrorMessage(dispatch_result.status_code);
      return dispatch_result;
    }
  }

  return SuccessExecutionResult();
}

ExecutionResult Dispatcher::ReloadCachedCodeObjects(
    shared_ptr<worker_api::WorkerApi>& worker) {
  auto all_cached_code_objects = code_object_cache_.GetAll();

  pending_requests_ += all_cached_code_objects.size();

  for (auto& kv : all_cached_code_objects) {
    auto& cached_code = kv.second;
    unique_ptr<CodeObject> ptr_cached_code;
    ptr_cached_code.reset(&cached_code);
    auto request_type = ptr_cached_code->js.empty()
                            ? constants::kRequestTypeWasm
                            : constants::kRequestTypeJavascript;
    if (!ptr_cached_code->wasm_bin.empty()) {
      request_type = constants::kRequestTypeJavascriptWithWasm;
    }
    auto run_code_request_or =
        request_converter::RequestConverter<CodeObject>::FromUserProvided(
            ptr_cached_code, request_type);

    if (!run_code_request_or.result().Successful()) {
      ptr_cached_code.release();
      pending_requests_ -= all_cached_code_objects.size();
      return run_code_request_or.result();
    }

    // Send the code objects to the worker again so it reloads its cache
    auto run_code_result_or = worker->RunCode(*run_code_request_or);
    if (!run_code_result_or.result().Successful()) {
      ptr_cached_code.release();
      pending_requests_ -= all_cached_code_objects.size();
      return run_code_result_or.result();
    }

    ptr_cached_code.release();
  }

  pending_requests_ -= all_cached_code_objects.size();

  return SuccessExecutionResult();
}
}  // namespace google::scp::roma::sandbox::dispatcher
