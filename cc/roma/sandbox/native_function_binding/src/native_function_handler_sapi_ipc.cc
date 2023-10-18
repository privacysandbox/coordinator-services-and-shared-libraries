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

#include "native_function_handler_sapi_ipc.h"

#include <memory>
#include <string>
#include <vector>

#include "roma/sandbox/constants/constants.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::roma::sandbox::constants::
    kFuctionBindingMetadataFunctionName;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

static constexpr char kFailedNativeHandlerExecution[] =
    "ROMA: Failed to execute the C++ function.";
static constexpr char kCouldNotFindFunctionName[] =
    "ROMA: Could not find C++ function by name.";

namespace google::scp::roma::sandbox::native_function_binding {
NativeFunctionHandlerSapiIpc::NativeFunctionHandlerSapiIpc(
    shared_ptr<NativeFunctionTable>& function_table, vector<int> local_fds,
    vector<int> remote_fds) {
  stop_ = false;
  function_table_ = function_table;
  auto process_count = local_fds.size();

  for (int i = 0; i < process_count; i++) {
    ipc_comms_.push_back(make_shared<sandbox2::Comms>(local_fds.at(i)));
  }

  remote_fds_ = remote_fds;
}

ExecutionResult NativeFunctionHandlerSapiIpc::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult NativeFunctionHandlerSapiIpc::Run() noexcept {
  for (int i = 0; i < ipc_comms_.size(); i++) {
    function_handler_threads_.emplace_back([this, i] {
      while (true) {
        auto comms = ipc_comms_.at(i);
        proto::FunctionBindingIoProto io_proto;

        // This unblocks once a call is issued from the other side
        bool received = comms->RecvProtoBuf(&io_proto);

        if (stop_.load()) {
          break;
        }

        if (!received) {
          continue;
        }

        // Get function name
        string function_name;
        if (io_proto.metadata().find(kFuctionBindingMetadataFunctionName) ==
            io_proto.metadata().end()) {
          // If we can't find the function, add errors to the proto to return
          io_proto.mutable_errors()->Add(kCouldNotFindFunctionName);
        } else {
          function_name =
              io_proto.metadata().at(kFuctionBindingMetadataFunctionName);
          if (!function_table_->Call(function_name, io_proto).Successful()) {
            // If execution failed, add errors to the proto to return
            io_proto.mutable_errors()->Add(kFailedNativeHandlerExecution);
          }
        }

        if (!comms->SendProtoBuf(io_proto)) {
          continue;
        }
      }
    });
  }

  return SuccessExecutionResult();
}

ExecutionResult NativeFunctionHandlerSapiIpc::Stop() noexcept {
  stop_.store(true);

  // We write to the comms object so that we can unblock the function binding
  // threads waiting on it.
  for (auto fd : remote_fds_) {
    sandbox2::Comms remote_comms(fd);
    proto::FunctionBindingIoProto io_proto;
    remote_comms.SendProtoBuf(io_proto);
  }

  // Wait for the function binding threads to stop before terminating the comms
  // object below.
  for (auto& t : function_handler_threads_) {
    if (t.joinable()) {
      t.join();
    }
  }

  for (auto& c : ipc_comms_) {
    c->Terminate();
  }

  return SuccessExecutionResult();
}
}  // namespace google::scp::roma::sandbox::native_function_binding
