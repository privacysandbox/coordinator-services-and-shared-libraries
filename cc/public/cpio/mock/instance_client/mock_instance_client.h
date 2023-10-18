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

#include <gmock/gmock.h>

#include <memory>

#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/instance_client/instance_client_interface.h"

namespace google::scp::cpio {
class MockInstanceClient : public InstanceClientInterface {
 public:
  MockInstanceClient() {
    ON_CALL(*this, Init)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Run)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
    ON_CALL(*this, Stop)
        .WillByDefault(testing::Return(core::SuccessExecutionResult()));
  }

  MOCK_METHOD(core::ExecutionResult, Init, (), (noexcept, override));
  MOCK_METHOD(core::ExecutionResult, Run, (), (noexcept, override));
  MOCK_METHOD(core::ExecutionResult, Stop, (), (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, GetCurrentInstanceResourceName,
      (cmrt::sdk::instance_service::v1::GetCurrentInstanceResourceNameRequest
           request,
       Callback<cmrt::sdk::instance_service::v1::
                    GetCurrentInstanceResourceNameResponse>
           callback),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, GetTagsByResourceName,
      (cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest request,
       Callback<cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>
           callback),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, GetInstanceDetailsByResourceName,
      (cmrt::sdk::instance_service::v1::GetInstanceDetailsByResourceNameRequest
           request,
       Callback<cmrt::sdk::instance_service::v1::
                    GetInstanceDetailsByResourceNameResponse>
           callback),
      (noexcept, override));
};

}  // namespace google::scp::cpio
