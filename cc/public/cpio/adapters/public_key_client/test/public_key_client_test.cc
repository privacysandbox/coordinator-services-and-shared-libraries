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

#include "public/cpio/adapters/public_key_client/src/public_key_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/public_key_client/mock/mock_public_key_client_with_overrides.h"
#include "public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "public/cpio/interface/public_key_client/type_def.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

using google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::PublicKeyClient;
using google::scp::cpio::PublicKeyClientOptions;
using google::scp::cpio::mock::MockPublicKeyClientWithOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::Return;

namespace google::scp::cpio::test {
class PublicKeyClientTest : public ::testing::Test {
 protected:
  PublicKeyClientTest() {
    auto public_key_client_options = make_shared<PublicKeyClientOptions>();
    client_ = make_unique<MockPublicKeyClientWithOverrides>(
        public_key_client_options);

    EXPECT_THAT(client_->Init(), IsSuccessful());
    EXPECT_THAT(client_->Run(), IsSuccessful());
  }

  ~PublicKeyClientTest() { EXPECT_THAT(client_->Stop(), IsSuccessful()); }

  unique_ptr<MockPublicKeyClientWithOverrides> client_;
};

TEST_F(PublicKeyClientTest, ListPublicKeysSuccess) {
  EXPECT_CALL(*client_->GetPublicKeyClientProvider(), ListPublicKeys)
      .WillOnce([=](AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse>&
                        context) {
        context.response = make_shared<ListPublicKeysResponse>();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> finished = false;
  EXPECT_THAT(client_->ListPublicKeys(ListPublicKeysRequest(),
                                      [&](const ExecutionResult result,
                                          ListPublicKeysResponse response) {
                                        EXPECT_THAT(result, IsSuccessful());
                                        finished = true;
                                      }),
              IsSuccessful());
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(PublicKeyClientTest, ListPublicKeysFailure) {
  EXPECT_CALL(*client_->GetPublicKeyClientProvider(), ListPublicKeys)
      .WillOnce([=](AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse>&
                        context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return FailureExecutionResult(SC_UNKNOWN);
      });

  atomic<bool> finished = false;
  EXPECT_THAT(
      client_->ListPublicKeys(
          ListPublicKeysRequest(),
          [&](const ExecutionResult result, ListPublicKeysResponse response) {
            EXPECT_THAT(result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            finished = true;
          }),
      ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(PublicKeyClientTest, FailureToCreatePublicKeyClientProvider) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  client_->create_public_key_client_provider_result = failure;
  EXPECT_EQ(client_->Init(), failure);
}
}  // namespace google::scp::cpio::test
