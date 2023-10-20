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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>

#include <google/protobuf/util/time_util.h>

#include "core/common/operation_dispatcher/src/error_codes.h"
#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/gcp_helper/gcp_helper.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/queue_service/v1/queue_service.grpc.pb.h"
#include "public/cpio/proto/queue_service/v1/queue_service.pb.h"

#include "test_gcp_sdk_server_starter.h"

using google::cmrt::sdk::queue_service::v1::EnqueueMessageRequest;
using google::cmrt::sdk::queue_service::v1::EnqueueMessageResponse;
using google::cmrt::sdk::queue_service::v1::QueueService;
using google::protobuf::MapPair;
using google::protobuf::util::TimeUtil;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::proto::EXECUTION_STATUS_SUCCESS;
using google::scp::core::test::CreatePublisherStub;
using google::scp::core::test::CreateTopic;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using grpc::CallbackServerContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerUnaryReactor;
using grpc::Status;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::mt19937;
using std::random_device;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

namespace {
constexpr char kLocalHost[] = "http://127.0.0.1";
// TODO(b/241857324): pick available ports randomly.
constexpr char kGcpPort[] = "4566";
constexpr char kSdkPort[] = "8888";

constexpr char kServerUri[] = "unix:///tmp/queue_service.socket";

constexpr char kSdkServerImageLocation[] =
    "cc/cpio/test/server/gcp/test_gcp_sdk_server_container.tar";
constexpr char kSdkServerImageName[] =
    "bazel/cc/cpio/test/server/gcp:test_gcp_sdk_server_container";
}  // namespace

namespace google::scp::cpio::test {
static string GetRandomString(const string& prefix) {
  // Bucket name can only be lower case.
  string str("abcdefghijklmnopqrstuvwxyz");
  static random_device random_device_local;
  static mt19937 generator(random_device_local());
  shuffle(str.begin(), str.end(), generator);
  return prefix + str.substr(0, 10);
}

static TestSdkServerConfig BuildTestSdkServerConfig() {
  TestSdkServerConfig config;
  config.network_name = GetRandomString("network");

  config.cloud_container_name = GetRandomString("gcp-container");
  config.cloud_port = kGcpPort;

  config.sdk_container_name = GetRandomString("sdk-container-");
  config.sdk_port = kSdkPort;

  config.queue_service_queue_name = GetRandomString("queue_name");

  return config;
}

static string CreateUrl(string host, string port) {
  return host + ":" + port;
}

class GcpSdkServerIntegrationTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    config_ = BuildTestSdkServerConfig();
    server_starter_ = make_unique<TestGcpSdkServerStarter>(config_);
    server_starter_->Setup();
    server_starter_->RunSdkServer(kSdkServerImageLocation, kSdkServerImageName,
                                  {});

    // Wait for the server is up.
    std::this_thread::sleep_for(std::chrono::seconds(10));
    // Grant permission to talk to the CPIO server outside the container.
    string log_command =
        string("docker container logs ") + config_.sdk_container_name;
    auto result = std::system(log_command.c_str());
    if (result != 0) {
      throw runtime_error("Failed to show log!");
    } else {
      std::cout << "Succeeded to show log!" << std::endl;
    }

    // Grant permission to talk to the CPIO server outside the container.
    string grant_permission_command =
        string("docker exec -itd ") + config_.sdk_container_name +
        string(" chmod 666 tmp/queue_service.socket");
    result = std::system(grant_permission_command.c_str());
    if (result != 0) {
      throw runtime_error("Failed to grant permission!");
    } else {
      std::cout << "Succeeded to grant permission!" << std::endl;
    }
    // Wait for the permission propagated.
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }

  static void TearDownTestSuite() {
    server_starter_->StopSdkServer();
    server_starter_->Teardown();
  }

  static TestSdkServerConfig config_;
  static unique_ptr<TestGcpSdkServerStarter> server_starter_;
};

TestSdkServerConfig GcpSdkServerIntegrationTest::config_ =
    TestSdkServerConfig();
unique_ptr<TestGcpSdkServerStarter>
    GcpSdkServerIntegrationTest::server_starter_ = nullptr;

TEST_F(GcpSdkServerIntegrationTest, QueueServiceEnqueueMessage) {
  // Setup test data.
  auto publisher_stub = CreatePublisherStub("0.0.0.0:" + config_.cloud_port);
  CreateTopic(*publisher_stub, "test-project",
              config_.queue_service_queue_name);

  auto channel =
      grpc::CreateChannel(kServerUri, grpc::InsecureChannelCredentials());
  auto stub = QueueService::NewStub(channel);

  EnqueueMessageRequest request;
  request.set_message_body("test message");
  EnqueueMessageResponse response;
  grpc::ClientContext ctx;
  auto status = stub->EnqueueMessage(&ctx, request, &response);
  EXPECT_TRUE(status.ok());
  if (!status.ok()) {
    std::cerr << "Grpc failed " << status.error_code() << ": "
              << status.error_message() << std::endl;
  }
  EXPECT_EQ(response.result().status(), EXECUTION_STATUS_SUCCESS);
  if (response.result().status() != EXECUTION_STATUS_SUCCESS) {
    std::cerr << "Error in the response: " << response.result().error_message()
              << std::endl;
  }
}
}  // namespace google::scp::cpio::test
