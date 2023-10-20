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
#include <grpcpp/health_check_service_interface.h>

#include <aws/core/Aws.h>
#include <google/protobuf/util/time_util.h>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/operation_dispatcher/src/error_codes.h"
#include "core/interface/errors.h"
#include "core/logger/src/log_providers/syslog/syslog_log_provider.h"
#include "core/logger/src/logger.h"
#include "core/test/utils/aws_helper/aws_helper.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/docker_helper/docker_helper.h"
#include "core/test/utils/logging_utils.h"
#include "cpio/server/interface/unix_socket_addresses.h"
#include "grpcpp/ext/proto_server_reflection_plugin.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/blob_storage_service/v1/blob_storage_service.grpc.pb.h"
#include "public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"
#include "public/cpio/proto/metric_service/v1/metric_service.grpc.pb.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.grpc.pb.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

#include "test_aws_sdk_server_starter.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::cmrt::sdk::blob_storage_service::v1::BlobStorageService;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::cmrt::sdk::metric_service::v1::MetricService;
using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::cmrt::sdk::parameter_service::v1::ParameterService;
using google::protobuf::MapPair;
using google::protobuf::util::TimeUtil;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::LoggerInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::proto::EXECUTION_STATUS_SUCCESS;
using google::scp::core::logger::Logger;
using google::scp::core::logger::log_providers::SyslogLogProvider;
using google::scp::core::test::CreateBucket;
using google::scp::core::test::CreateS3Client;
using google::scp::core::test::CreateSSMClient;
using google::scp::core::test::GrantPermissionToFolder;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::PutParameter;
using google::scp::core::test::ResultIs;
using google::scp::core::test::TestLoggingUtils;
using google::scp::core::test::TestTimeoutException;
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
constexpr char kSdkServerImageLocation[] =
    "cc/cpio/test/server/aws/test_aws_sdk_server_container.tar";
constexpr char kSdkServerImageName[] =
    "bazel/cc/cpio/test/server/aws:test_aws_sdk_server_container";

constexpr char kRegion[] = "us-east-1";
// TODO(b/241857324): pick available ports randomly.
constexpr char kLocalstackPort[] = "4566";
constexpr char kSdkPort[] = "8888";

constexpr int kNumThreads = 1;
const MetricUnit kUnit = MetricUnit::METRIC_UNIT_COUNT;
constexpr char kNamespace[] = "MetricClientTest";
constexpr char kName[] = "BlockingMetricServiceTest";
constexpr char kValue[] = "2";
constexpr char kLabelKey1[] = "key1";
constexpr char kLabelKey2[] = "key2";
constexpr char kLabelValue1[] = "value1";
constexpr char kLabelValue2[] = "value2";
constexpr char kBucketName[] = "blob-storage-service-test-bucket";
constexpr char kBlobName[] = "blob_name";
constexpr char kBlobData[] = "some sample data";
constexpr char kParameterName[] = "test_parameter_name";
constexpr char kParameterValue[] = "test_parameter_value";

void CreatePutMetricsRequest(PutMetricsRequest& put_metrics_request) {
  put_metrics_request.set_metric_namespace(kNamespace);
  auto* custom_metric = put_metrics_request.add_metrics();
  custom_metric->set_name(kName);
  custom_metric->set_value(kValue);
  custom_metric->set_unit(kUnit);
  *custom_metric->mutable_timestamp() = TimeUtil::GetCurrentTime();

  auto* metric_labels = custom_metric->mutable_labels();
  metric_labels->insert(MapPair(string(kLabelKey1), string(kLabelValue1)));
  metric_labels->insert(MapPair(string(kLabelKey2), string(kLabelValue2)));
}
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
  config.region = kRegion;
  config.network_name = GetRandomString("network");

  config.cloud_container_name = GetRandomString("localstack-container");
  config.cloud_port = kLocalstackPort;

  config.sdk_container_name = GetRandomString("sdk-container-");
  config.sdk_port = kSdkPort;

  config.job_service_queue_name = GetRandomString("job_queue");
  config.job_service_table_name = GetRandomString("job_table");
  config.queue_service_queue_name = GetRandomString("queue_name");

  return config;
}

class AwsSdkServerIntegrationTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
    config_ = BuildTestSdkServerConfig();
    server_starter_ = make_unique<TestAwsSdkServerStarter>(config_);
    server_starter_->Setup();
    server_starter_->RunSdkServer(kSdkServerImageLocation, kSdkServerImageName,
                                  {});

    std::this_thread::sleep_for(std::chrono::seconds(10));
    string s = string("docker container logs ") + config_.sdk_container_name;
    std::system(s.c_str());
    GrantPermissionToFolder(config_.sdk_container_name,
                            "tmp/metric_service.socket");
    GrantPermissionToFolder(config_.sdk_container_name,
                            "tmp/blob_storage_service.socket");
    GrantPermissionToFolder(config_.sdk_container_name,
                            "tmp/parameter_service.socket");
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }

  static void TearDownTestSuite() {
    server_starter_->StopSdkServer();
    server_starter_->Teardown();
    SDKOptions options;
    ShutdownAPI(options);
  }

  string localstack_endpoint =
      string(kLocalHost) + ":" + string(kLocalstackPort);
  static TestSdkServerConfig config_;
  static unique_ptr<TestAwsSdkServerStarter> server_starter_;
};

TestSdkServerConfig AwsSdkServerIntegrationTest::config_ =
    TestSdkServerConfig();
unique_ptr<TestAwsSdkServerStarter>
    AwsSdkServerIntegrationTest::server_starter_ = nullptr;

TEST_F(AwsSdkServerIntegrationTest, MetricServicePutMetrics) {
  auto channel = grpc::CreateChannel(kMetricServiceAddress,
                                     grpc::InsecureChannelCredentials());
  auto stub = MetricService::NewStub(channel);

  auto work_threads = std::vector<std::thread>();
  work_threads.reserve(kNumThreads);

  for (int i = 0; i < kNumThreads; i++) {
    work_threads.push_back(std::thread([&] {
      PutMetricsRequest request;
      CreatePutMetricsRequest(request);
      PutMetricsResponse response;
      grpc::ClientContext ctx;
      auto status = stub->PutMetrics(&ctx, request, &response);
      EXPECT_TRUE(status.ok())
          << "PutMetrics Grpc failed " << status.error_code() << ": "
          << status.error_message() << std::endl;
      EXPECT_EQ(response.result().status(), EXECUTION_STATUS_SUCCESS)
          << "PutMetrics failed: " << response.result().error_message()
          << std::endl;
    }));
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}

TEST_F(AwsSdkServerIntegrationTest,
       BlobStorageClientPutAndGetBlobSuccessfully) {
  // Setup test data.
  auto s3_client = CreateS3Client(localstack_endpoint);
  CreateBucket(s3_client, kBucketName);

  auto channel = grpc::CreateChannel(kBlobStorageServiceAddress,
                                     grpc::InsecureChannelCredentials());
  auto stub = BlobStorageService::NewStub(channel);

  // Test PutBlob.
  auto put_blob_request = PutBlobRequest();
  put_blob_request.mutable_blob()->mutable_metadata()->set_bucket_name(
      kBucketName);
  put_blob_request.mutable_blob()->mutable_metadata()->set_blob_name(kBlobName);
  put_blob_request.mutable_blob()->set_data(kBlobData);

  PutBlobResponse put_blob_response;
  grpc::ClientContext put_blob_ctx;
  auto status =
      stub->PutBlob(&put_blob_ctx, put_blob_request, &put_blob_response);
  EXPECT_TRUE(status.ok()) << "PutBlob Grpc failed " << status.error_code()
                           << ": " << status.error_message() << std::endl;
  EXPECT_EQ(put_blob_response.result().status(), EXECUTION_STATUS_SUCCESS)
      << "PutBlobRequest failed: " << put_blob_response.result().error_message()
      << std::endl;

  // Test GetBlob.
  auto get_blob_request = GetBlobRequest();
  get_blob_request.mutable_blob_metadata()->set_bucket_name(kBucketName);
  get_blob_request.mutable_blob_metadata()->set_blob_name(kBlobName);

  GetBlobResponse get_blob_response;
  grpc::ClientContext get_blob_ctx;
  status = stub->GetBlob(&get_blob_ctx, get_blob_request, &get_blob_response);
  EXPECT_TRUE(status.ok()) << "GetBlob Grpc failed " << status.error_code()
                           << ": " << status.error_message() << std::endl;
  EXPECT_EQ(get_blob_response.result().status(), EXECUTION_STATUS_SUCCESS)
      << "GetBlob failed: " << get_blob_response.result().error_message()
      << std::endl;
  EXPECT_EQ(get_blob_response.blob().data(), kBlobData);
}

TEST_F(AwsSdkServerIntegrationTest, GetParameterSuccessfully) {
  // Setup test data.
  auto ssm_client = CreateSSMClient(localstack_endpoint);
  PutParameter(ssm_client, kParameterName, kParameterValue);

  auto channel = grpc::CreateChannel(kParameterServiceAddress,
                                     grpc::InsecureChannelCredentials());
  auto stub = ParameterService::NewStub(channel);

  GetParameterRequest get_parameter_request;
  get_parameter_request.set_parameter_name(kParameterName);
  GetParameterResponse get_parameter_response;
  grpc::ClientContext get_parameter_ctx;
  auto status = stub->GetParameter(&get_parameter_ctx, get_parameter_request,
                                   &get_parameter_response);
  EXPECT_TRUE(status.ok()) << "GetParameter Grpc failed " << status.error_code()
                           << ": " << status.error_message() << std::endl;
  EXPECT_EQ(get_parameter_response.result().status(), EXECUTION_STATUS_SUCCESS)
      << "GetParameter failed: "
      << get_parameter_response.result().error_message() << std::endl;
  EXPECT_EQ(get_parameter_response.parameter_value(), kParameterValue);
}
}  // namespace google::scp::cpio::test
