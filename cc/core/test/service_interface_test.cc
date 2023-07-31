// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "core/interface/service_interface.h"

#include <gtest/gtest.h>

#include <memory>

using google::scp::core::ExecutionResult;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;

namespace google::scp::core::test {

class DummyService : public ServiceInterface {
 public:
  DummyService() : state_(State::kBad) {}

  ExecutionResult Init() noexcept {
    state_ = State::kInit;
    return SuccessExecutionResult();
  }

  ExecutionResult Run() noexcept {
    state_ = State::kRunning;
    return SuccessExecutionResult();
  }

  ExecutionResult Stop() noexcept {
    state_ = State::kStopped;
    return SuccessExecutionResult();
  }

 private:
  enum class State {
    kBad = 0,
    kInit,
    kRunning,
    kStopped,
  };
  State state_;
};

TEST(ServiceInterface, DummyService) {
  DummyService dummy;
  auto p = std::make_shared<DummyService>();
}

}  // namespace google::scp::core::test
