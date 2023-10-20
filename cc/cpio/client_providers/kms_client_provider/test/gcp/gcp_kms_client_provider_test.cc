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

#include "cpio/client_providers/kms_client_provider/src/gcp/gcp_kms_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "core/utils/src/base64.h"
#include "cpio/client_providers/kms_client_provider/mock/gcp/mock_gcp_key_management_service_client.h"
#include "cpio/client_providers/kms_client_provider/src/gcp/error_codes.h"
#include "cpio/client_providers/kms_client_provider/src/gcp/gcp_kms_aead.h"
#include "public/core/test/interface/execution_result_matchers.h"

using crypto::tink::Aead;
using crypto::tink::util::StatusOr;
using google::cloud::Status;
using google::cloud::StatusCode;
using GcsDecryptRequest = google::cloud::kms::v1::DecryptRequest;
using GcsDecryptResponse = google::cloud::kms::v1::DecryptResponse;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::
    SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED;
using google::scp::core::errors::
    SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::client_providers::mock::
    MockGcpKeyManagementServiceClient;
using std::atomic;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::Return;

static constexpr char kServiceAccount[] = "account";
static constexpr char kWipProvider[] = "wip";
static constexpr char kKeyArn[] = "keyArn";
static constexpr char kCiphertext[] = "ciphertext";
static constexpr char kPlaintext[] = "plaintext";

namespace google::scp::cpio::client_providers::test {
class MockGcpKmsAeadProvider : public GcpKmsAeadProvider {
 public:
  MOCK_METHOD(ExecutionResultOr<shared_ptr<Aead>>, CreateAead,
              (const string&, const string&, const string&),
              (noexcept, override));
};

class GcpKmsClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_aead_provider_ = make_shared<MockGcpKmsAeadProvider>();
    client_ = make_unique<GcpKmsClientProvider>(mock_aead_provider_);
    EXPECT_SUCCESS(client_->Init());
    EXPECT_SUCCESS(client_->Run());

    mock_gcp_key_management_service_client_ =
        std::make_shared<MockGcpKeyManagementServiceClient>();
    crypto::tink::util::StatusOr<std::unique_ptr<crypto::tink::Aead>>
        aead_result = GcpKmsAead::New(
            kKeyArn,
            std::dynamic_pointer_cast<GcpKeyManagementServiceClientInterface>(
                mock_gcp_key_management_service_client_));
    aead_ = std::move(*aead_result);
  }

  void TearDown() override { EXPECT_SUCCESS(client_->Stop()); }

  unique_ptr<GcpKmsClientProvider> client_;
  shared_ptr<MockGcpKmsAeadProvider> mock_aead_provider_;
  shared_ptr<MockGcpKeyManagementServiceClient>
      mock_gcp_key_management_service_client_;
  shared_ptr<Aead> aead_;
};

TEST_F(GcpKmsClientProviderTest, NullKeyArn) {
  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_ciphertext(kCiphertext);

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {});

  EXPECT_THAT(client_->Decrypt(context),
              ResultIs(FailureExecutionResult(
                  SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND)));
}

TEST_F(GcpKmsClientProviderTest, EmptyKeyArn) {
  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name("");
  kms_decrpyt_request->set_ciphertext(kCiphertext);

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {});

  EXPECT_THAT(client_->Decrypt(context),
              ResultIs(FailureExecutionResult(
                  SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND)));
}

TEST_F(GcpKmsClientProviderTest, NullCiphertext) {
  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {});

  EXPECT_THAT(client_->Decrypt(context),
              ResultIs(FailureExecutionResult(
                  SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND)));
}

TEST_F(GcpKmsClientProviderTest, EmptyCiphertext) {
  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  kms_decrpyt_request->set_ciphertext("");

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {});

  EXPECT_THAT(client_->Decrypt(context),
              ResultIs(FailureExecutionResult(
                  SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND)));
}

MATCHER_P(RequestMatches, req, "") {
  return ExplainMatchResult(Eq(req.name()), arg.name(), result_listener) &&
         ExplainMatchResult(Eq(req.ciphertext()), arg.ciphertext(),
                            result_listener) &&
         ExplainMatchResult(Eq(req.additional_authenticated_data()),
                            arg.additional_authenticated_data(),
                            result_listener);
}

TEST_F(GcpKmsClientProviderTest, FailedToDecode) {
  EXPECT_CALL(*mock_aead_provider_,
              CreateAead(kWipProvider, kServiceAccount, kKeyArn))
      .WillOnce(Return(ExecutionResultOr<shared_ptr<Aead>>(aead_)));

  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  kms_decrpyt_request->set_ciphertext("abc");
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  atomic<bool> condition = false;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED)));
        condition = true;
      });

  EXPECT_THAT(client_->Decrypt(context),
              FailureExecutionResult(
                  SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED));

  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpKmsClientProviderTest, SuccessToDecrypt) {
  EXPECT_CALL(*mock_aead_provider_,
              CreateAead(kWipProvider, kServiceAccount, kKeyArn))
      .WillOnce(Return(ExecutionResultOr<shared_ptr<Aead>>(aead_)));

  string encoded_ciphertext;
  ASSERT_THAT(Base64Encode(kCiphertext, encoded_ciphertext), IsSuccessful());

  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  kms_decrpyt_request->set_ciphertext(encoded_ciphertext);
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  GcsDecryptRequest decrypt_request;
  decrypt_request.set_name(string(kKeyArn));
  decrypt_request.set_ciphertext(string(kCiphertext));
  GcsDecryptResponse decrypt_response;
  decrypt_response.set_plaintext(kPlaintext);
  EXPECT_CALL(*mock_gcp_key_management_service_client_,
              Decrypt(RequestMatches(decrypt_request)))
      .WillOnce(Return(decrypt_response));

  atomic<bool> condition = false;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->plaintext(), kPlaintext);
        condition = true;
      });

  EXPECT_SUCCESS(client_->Decrypt(context));

  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpKmsClientProviderTest, FailedToDecrypt) {
  EXPECT_CALL(*mock_aead_provider_,
              CreateAead(kWipProvider, kServiceAccount, kKeyArn))
      .WillOnce(Return(ExecutionResultOr<shared_ptr<Aead>>(aead_)));

  string encoded_ciphertext;
  Base64Encode(kCiphertext, encoded_ciphertext);
  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  kms_decrpyt_request->set_ciphertext(encoded_ciphertext);
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  GcsDecryptRequest decrypt_request;
  decrypt_request.set_name(string(kKeyArn));
  decrypt_request.set_ciphertext(string(kCiphertext));
  GcsDecryptResponse decrypt_response;
  decrypt_response.set_plaintext(kPlaintext);
  EXPECT_CALL(*mock_gcp_key_management_service_client_,
              Decrypt(RequestMatches(decrypt_request)))
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "Invalid input")));

  atomic<bool> condition = false;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED)));
        condition = true;
      });

  EXPECT_THAT(client_->Decrypt(context),
              ResultIs(FailureExecutionResult(
                  SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED)));

  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
