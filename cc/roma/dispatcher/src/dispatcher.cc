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

#include "dispatcher.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "roma/ipc/src/ipc_manager.h"

using absl::Status;
using absl::StatusCode;
using absl::StatusOr;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::roma::common::RoleId;
using google::scp::roma::ipc::IpcChannel;
using google::scp::roma::ipc::IpcManager;
using google::scp::roma::ipc::Request;
using google::scp::roma::ipc::RequestType;
using google::scp::roma::ipc::Response;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_ptr;
using std::vector;

namespace google::scp::roma::dispatcher {

ExecutionResult Dispatcher::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult Dispatcher::Run() noexcept {
  auto num_workers = ipc_manager_.GetNumProcesses();
  stop_.store(false);
  response_pollers_.reserve(num_workers);
  for (auto i = 0u; i < num_workers; ++i) {
    response_pollers_.emplace_back(
        thread(&Dispatcher::ResponsePollerWorker, this, i));
  }
  return SuccessExecutionResult();
}

ExecutionResult Dispatcher::Stop() noexcept {
  stop_.store(true);

  for (auto& t : response_pollers_) {
    t.join();
  }
  // TODO: drain the IpcChannels
  return SuccessExecutionResult();
}

ExecutionResult Dispatcher::Broadcast(unique_ptr<CodeObject> code_object,
                                      Callback broadcast_callback) noexcept {
  auto workers_num = ipc_manager_.GetNumProcesses();
  auto finished_counter = make_shared<atomic<size_t>>(0);
  auto responses_storage =
      make_shared<vector<unique_ptr<StatusOr<ResponseObject>>>>(workers_num);
  // Iterate over all workers and push code_object.
  for (size_t worker_index = 0; worker_index < workers_num; worker_index++) {
    auto callback =
        [workers_num, responses_storage, finished_counter, broadcast_callback,
         worker_index](unique_ptr<StatusOr<ResponseObject>> response) {
          auto& all_resp = *responses_storage;
          // Store responses in the vector
          all_resp[worker_index].swap(response);
          auto finished_value = finished_counter->fetch_add(1);
          // Go through the responses and call the callback on the first failed
          // one. If all succeeded, call on 0th.
          if (finished_value + 1 == workers_num) {
            for (auto& resp : all_resp) {
              if (!resp->ok()) {
                broadcast_callback(::std::move(resp));
                return;
              }
            }
            broadcast_callback(::std::move(all_resp[0]));
          }
        };

    // Switch to the appropriate IpcChannel and mempool
    auto ctx =
        ipc_manager_.SwitchTo(RoleId(worker_index, true /* dispatcher */));
    auto request =
        make_unique<Request>(make_unique<CodeObject>(*code_object), callback);
    request->type = RequestType::kUpdate;
    auto result = ipc_manager_.GetIpcChannel().PushRequest(move(request));
    if (!result.Successful()) {
      return result;
    }
  }
  return SuccessExecutionResult();
}

void Dispatcher::ResponsePollerWorker(uint32_t worker_index) noexcept {
  auto result = ipc_manager_.SetUpIpcForMyThread(
      RoleId(worker_index, true /* is_dispatcher */));
  if (!result.Successful()) {
    // TODO: handle failure
  }
  auto& ipc_channel = ipc_manager_.GetIpcChannel();
  while (!stop_.load()) {
    unique_ptr<Response> response;

    // PopResponse is a blocking call and will always return success when there
    // is a response. However, when stopping, it will return a Failure. So we
    // continue to let this main loop exit by evaluating the stop flag.
    auto result = ipc_channel.PopResponse(response);
    if (!result.Successful()) {
      continue;
    }

    unique_ptr<StatusOr<ResponseObject>> resp_arg;
    if (response->result.Successful()) {
      resp_arg =
          make_unique<StatusOr<ResponseObject>>(response->CreateCodeResponse());
    } else {
      resp_arg = make_unique<StatusOr<ResponseObject>>(
          Status(StatusCode::kInternal,
                 GetErrorMessage(response->result.status_code)));
    }
    // "move" by itself is ambiguous as absl defines it as well.
    (*response->request->callback)(::std::move(resp_arg));
  }
}

}  // namespace google::scp::roma::dispatcher
