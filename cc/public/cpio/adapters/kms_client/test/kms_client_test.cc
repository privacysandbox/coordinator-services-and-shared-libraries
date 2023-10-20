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

#include "public/cpio/adapters/kms_client/src/kms_client.h"

#include <gtest/gtest.h>

#include <atomic>

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/kms_client/mock/mock_kms_client_with_overrides.h"
#include "public/cpio/interface/kms_client/kms_client_interface.h"
#include "public/cpio/proto/kms_service/v1/kms_service.pb.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::cpio::mock::MockKmsClientWithOverrides;
using std::atomic_bool;

namespace google::scp::cpio::test {

class KmsClientTest : public ::testing::Test {
 protected:
  KmsClientTest() { assert(client_.Init().Successful()); }

  MockKmsClientWithOverrides client_;
};

TEST_F(KmsClientTest, DecryptSuccess) {
  AsyncContext<DecryptRequest, DecryptResponse> context;
  auto& client_provider = client_.GetKmsClientProvider();
  atomic_bool called(false);

  EXPECT_CALL(client_provider, Decrypt).WillOnce([&called](auto context) {
    called = true;
    return SuccessExecutionResult();
  });
  EXPECT_THAT(client_.Decrypt(context), IsSuccessful());
  EXPECT_TRUE(called);
}

}  // namespace google::scp::cpio::test
