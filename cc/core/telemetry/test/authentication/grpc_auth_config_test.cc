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

#include "core/telemetry/src/authentication/grpc_auth_config.h"

#include <memory>

#include "include/gtest/gtest.h"

namespace google::scp::core {
namespace {

class GrpcAuthConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auth_config_ = std::make_unique<GrpcAuthConfig>("test-service-account",
                                                    "test-audience");
  }

  std::unique_ptr<GrpcAuthConfig> auth_config_;
};

TEST_F(GrpcAuthConfigTest, DefaultConfigIsInvalid) {
  EXPECT_TRUE(auth_config_->IsValid());
}
}  // namespace
}  // namespace google::scp::core
