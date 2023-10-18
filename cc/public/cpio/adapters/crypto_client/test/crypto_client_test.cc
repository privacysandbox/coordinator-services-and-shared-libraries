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
// WITHOUT WARRANTIES OR finishedS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "public/cpio/adapters/crypto_client/src/crypto_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/crypto_client/mock/mock_crypto_client_with_overrides.h"
#include "public/cpio/interface/crypto_client/crypto_client_interface.h"
#include "public/cpio/interface/crypto_client/type_def.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

using google::cmrt::sdk::crypto_service::v1::AeadDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::CryptoClient;
using google::scp::cpio::CryptoClientOptions;
using google::scp::cpio::client_providers::mock::MockCryptoClientProvider;
using google::scp::cpio::mock::MockCryptoClientWithOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::Return;

namespace google::scp::cpio::test {
class CryptoClientTest : public ::testing::Test {
 protected:
  CryptoClientTest() {
    auto crypto_client_options = make_shared<CryptoClientOptions>();
    client_ = make_unique<MockCryptoClientWithOverrides>(crypto_client_options);

    EXPECT_CALL(*client_->GetCryptoClientProvider(), Init)
        .WillOnce(Return(SuccessExecutionResult()));
    EXPECT_CALL(*client_->GetCryptoClientProvider(), Run)
        .WillOnce(Return(SuccessExecutionResult()));
    EXPECT_CALL(*client_->GetCryptoClientProvider(), Stop)
        .WillOnce(Return(SuccessExecutionResult()));

    EXPECT_THAT(client_->Init(), IsSuccessful());
    EXPECT_THAT(client_->Run(), IsSuccessful());
  }

  ~CryptoClientTest() { EXPECT_THAT(client_->Stop(), IsSuccessful()); }

  unique_ptr<MockCryptoClientWithOverrides> client_;
};

TEST_F(CryptoClientTest, HpkeEncryptSuccess) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), HpkeEncrypt)
      .WillOnce(
          [=](AsyncContext<HpkeEncryptRequest, HpkeEncryptResponse>& context) {
            context.response = make_shared<HpkeEncryptResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });

  atomic<bool> finished = false;
  EXPECT_THAT(client_->HpkeEncrypt(HpkeEncryptRequest(),
                                   [&](const ExecutionResult result,
                                       HpkeEncryptResponse response) {
                                     EXPECT_THAT(result, IsSuccessful());
                                     finished = true;
                                   }),
              IsSuccessful());
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(CryptoClientTest, HpkeEncryptFailure) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), HpkeEncrypt)
      .WillOnce(
          [=](AsyncContext<HpkeEncryptRequest, HpkeEncryptResponse>& context) {
            context.result = FailureExecutionResult(SC_UNKNOWN);
            context.Finish();
            return FailureExecutionResult(SC_UNKNOWN);
          });

  atomic<bool> finished = false;
  EXPECT_THAT(
      client_->HpkeEncrypt(
          HpkeEncryptRequest(),
          [&](const ExecutionResult result, HpkeEncryptResponse response) {
            EXPECT_THAT(result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            finished = true;
          }),
      ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(CryptoClientTest, HpkeDecryptSuccess) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), HpkeDecrypt)
      .WillOnce(
          [=](AsyncContext<HpkeDecryptRequest, HpkeDecryptResponse>& context) {
            context.response = make_shared<HpkeDecryptResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });

  atomic<bool> finished = false;
  EXPECT_THAT(client_->HpkeDecrypt(HpkeDecryptRequest(),
                                   [&](const ExecutionResult result,
                                       HpkeDecryptResponse response) {
                                     EXPECT_THAT(result, IsSuccessful());
                                     finished = true;
                                   }),
              IsSuccessful());
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(CryptoClientTest, HpkeDecryptFailure) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), HpkeDecrypt)
      .WillOnce(
          [=](AsyncContext<HpkeDecryptRequest, HpkeDecryptResponse>& context) {
            context.result = FailureExecutionResult(SC_UNKNOWN);
            context.Finish();
            return FailureExecutionResult(SC_UNKNOWN);
          });

  atomic<bool> finished = false;
  EXPECT_THAT(
      client_->HpkeDecrypt(
          HpkeDecryptRequest(),
          [&](const ExecutionResult result, HpkeDecryptResponse response) {
            EXPECT_THAT(result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            finished = true;
          }),
      ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(CryptoClientTest, AeadEncryptSuccess) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), AeadEncrypt)
      .WillOnce(
          [=](AsyncContext<AeadEncryptRequest, AeadEncryptResponse>& context) {
            context.response = make_shared<AeadEncryptResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });

  atomic<bool> finished = false;
  EXPECT_THAT(client_->AeadEncrypt(AeadEncryptRequest(),
                                   [&](const ExecutionResult result,
                                       AeadEncryptResponse response) {
                                     EXPECT_THAT(result, IsSuccessful());
                                     finished = true;
                                   }),
              IsSuccessful());
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(CryptoClientTest, AeadEncryptFailure) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), AeadEncrypt)
      .WillOnce(
          [=](AsyncContext<AeadEncryptRequest, AeadEncryptResponse>& context) {
            context.result = FailureExecutionResult(SC_UNKNOWN);
            context.Finish();
            return FailureExecutionResult(SC_UNKNOWN);
          });

  atomic<bool> finished = false;
  EXPECT_THAT(
      client_->AeadEncrypt(
          AeadEncryptRequest(),
          [&](const ExecutionResult result, AeadEncryptResponse response) {
            EXPECT_THAT(result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            finished = true;
          }),
      ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(CryptoClientTest, AeadDecryptSuccess) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), AeadDecrypt)
      .WillOnce(
          [=](AsyncContext<AeadDecryptRequest, AeadDecryptResponse>& context) {
            context.response = make_shared<AeadDecryptResponse>();
            context.result = SuccessExecutionResult();
            context.Finish();
            return SuccessExecutionResult();
          });

  atomic<bool> finished = false;
  EXPECT_THAT(client_->AeadDecrypt(AeadDecryptRequest(),
                                   [&](const ExecutionResult result,
                                       AeadDecryptResponse response) {
                                     EXPECT_THAT(result, IsSuccessful());
                                     finished = true;
                                   }),
              IsSuccessful());
  WaitUntil([&]() { return finished.load(); });
}

TEST_F(CryptoClientTest, AeadDecryptFailure) {
  EXPECT_CALL(*client_->GetCryptoClientProvider(), AeadDecrypt)
      .WillOnce(
          [=](AsyncContext<AeadDecryptRequest, AeadDecryptResponse>& context) {
            context.result = FailureExecutionResult(SC_UNKNOWN);
            context.Finish();
            return FailureExecutionResult(SC_UNKNOWN);
          });

  atomic<bool> finished = false;
  EXPECT_THAT(
      client_->AeadDecrypt(
          AeadDecryptRequest(),
          [&](const ExecutionResult result, AeadDecryptResponse response) {
            EXPECT_THAT(result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            finished = true;
          }),
      ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  WaitUntil([&]() { return finished.load(); });
}
}  // namespace google::scp::cpio::test
