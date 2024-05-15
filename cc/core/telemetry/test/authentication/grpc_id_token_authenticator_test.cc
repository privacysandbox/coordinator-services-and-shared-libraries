//  Copyright 2024 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "core/telemetry/src/authentication/grpc_id_token_authenticator.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace google::scp::core {
namespace {
inline constexpr std::string_view kExpectedToken = "default_token";

class MockTokenFetcher : public TokenFetcher {
 public:
  MockTokenFetcher() {
    ON_CALL(*this, FetchIdToken(testing::_))
        .WillByDefault(testing::Return(
            ExecutionResultOr<std::string>((std::string)kExpectedToken)));
  }

  MOCK_METHOD(ExecutionResultOr<std::string>, FetchIdToken, (GrpcAuthConfig&),
              (override));
};

class FakeAuthContext : public grpc::AuthContext {
 public:
  class Iterator : public grpc::AuthPropertyIterator {
   public:
    Iterator() : grpc::AuthPropertyIterator(nullptr, nullptr) {}
  };

  bool IsPeerAuthenticated() const override { return true; }

  std::vector<grpc::string_ref> GetPeerIdentity() const override { return {}; }

  std::string GetPeerIdentityPropertyName() const override { return ""; }

  std::vector<grpc::string_ref> FindPropertyValues(
      const std::string& name) const override {
    return {};
  }

  grpc::AuthPropertyIterator begin() const override { return Iterator(); }

  grpc::AuthPropertyIterator end() const override { return Iterator(); }

  void AddProperty(const std::string& key,
                   const grpc::string_ref& value) override {}

  bool SetPeerIdentityPropertyName(const std::string& name) override {
    return true;
  }
};

class GrpcIdTokenAuthenticatorTest : public ::testing::Test {
 protected:
  std::unique_ptr<GrpcAuthConfig> auth_config;
  std::unique_ptr<MockTokenFetcher> token_fetcher;
  std::unique_ptr<GrpcIdTokenAuthenticator> authenticator;

  void SetUp() override {
    auth_config = std::make_unique<GrpcAuthConfig>("service_account",
                                                   "audience", "cred_config");
    token_fetcher = std::make_unique<MockTokenFetcher>();
    authenticator = std::make_unique<GrpcIdTokenAuthenticator>(
        std::move(auth_config), std::move(token_fetcher));
  }
};

TEST_F(GrpcIdTokenAuthenticatorTest, GetMetadata_ExpiredToken_RefreshesToken) {
  authenticator->set_expiry_time_for_testing(std::chrono::system_clock::now());

  grpc::string_ref service_url = "";
  grpc::string_ref method_name = "";
  FakeAuthContext channel_auth_context;
  std::multimap<grpc::string, grpc::string> metadata;

  grpc::Status status = authenticator->GetMetadata(
      service_url, method_name, channel_auth_context, &metadata);

  // Assert
  ASSERT_TRUE(status.ok());
  auto it = metadata.find("authorization");
  ASSERT_NE(it, metadata.end());
  EXPECT_EQ(it->second, "Bearer " + (std::string)kExpectedToken);
}

TEST_F(GrpcIdTokenAuthenticatorTest,
       GetMetadata_NonExpiredToken_UsesExistingToken) {
  // Set the token to be non-expired
  authenticator->set_expiry_time_for_testing(std::chrono::system_clock::now() +
                                             std::chrono::seconds(3600));
  authenticator->set_id_token("unexpired_token");

  grpc::string_ref service_url = "";
  grpc::string_ref method_name = "";
  FakeAuthContext channel_auth_context;
  std::multimap<grpc::string, grpc::string> metadata;

  grpc::Status status = authenticator->GetMetadata(
      service_url, method_name, channel_auth_context, &metadata);

  // Assert
  ASSERT_TRUE(status.ok());
  auto it = metadata.find("authorization");
  ASSERT_NE(it, metadata.end());
  EXPECT_EQ(it->second, "Bearer unexpired_token");

  testing::Mock::VerifyAndClearExpectations(token_fetcher.get());
}

TEST_F(GrpcIdTokenAuthenticatorTest, IsExpired_ChecksTokenExpiryCorrectly) {
  // Set token to expire in the future
  authenticator->set_expiry_time_for_testing(std::chrono::system_clock::now() +
                                             std::chrono::minutes(5));
  EXPECT_FALSE(authenticator->IsExpired());

  // Set token to have expired
  authenticator->set_expiry_time_for_testing(std::chrono::system_clock::now() -
                                             std::chrono::minutes(5));
  EXPECT_TRUE(authenticator->IsExpired());
}
}  // namespace
}  // namespace google::scp::core
