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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "core/test/utils/aws_helper/aws_helper.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/docker_helper/docker_helper.h"
#include "core/utils/src/base64.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/blob_storage_client/test/test_aws_blob_storage_client.h"
#include "public/cpio/adapters/kms_client/test/test_aws_kms_client.h"
#include "public/cpio/adapters/metric_client/test/test_aws_metric_client.h"
#include "public/cpio/adapters/parameter_client/test/test_aws_parameter_client.h"
#include "public/cpio/test/blob_storage_client/test_aws_blob_storage_client_options.h"
#include "public/cpio/test/global_cpio/test_cpio_options.h"
#include "public/cpio/test/global_cpio/test_lib_cpio.h"
#include "public/cpio/test/kms_client/test_aws_kms_client_options.h"
#include "public/cpio/test/parameter_client/test_aws_parameter_client_options.h"

using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::protobuf::MapPair;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::test::CreateBucket;
using google::scp::core::test::CreateKey;
using google::scp::core::test::CreateKMSClient;
using google::scp::core::test::CreateS3Client;
using google::scp::core::test::CreateSSMClient;
using google::scp::core::test::Encrypt;
using google::scp::core::test::GetParameter;
using google::scp::core::test::PutParameter;
using google::scp::core::test::StartLocalStackContainer;
using google::scp::core::test::StopContainer;
using google::scp::core::test::WaitUntil;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::TestAwsBlobStorageClientOptions;
using google::scp::cpio::TestAwsKmsClient;
using google::scp::cpio::TestAwsKmsClientOptions;
using google::scp::cpio::TestAwsMetricClient;
using google::scp::cpio::TestAwsMetricClientOptions;
using google::scp::cpio::TestAwsParameterClient;
using google::scp::cpio::TestAwsParameterClientOptions;
using google::scp::cpio::TestCpioOptions;
using google::scp::cpio::TestLibCpio;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_ptr;
using std::vector;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace {
constexpr char kLocalHost[] = "http://127.0.0.1";
constexpr char kLocalstackContainerName[] = "cpio_integration_test_localstack";
// TODO(b/241857324): pick available ports randomly.
constexpr char kLocalstackPort[] = "8888";
constexpr char kParameterName[] = "test_parameter_name";
constexpr char kParameterValue[] = "test_parameter_value";
constexpr char kBucketName[] = "blob-storage-service-test-bucket";
constexpr char kBlobName[] = "blob_name";
constexpr char kBlobData[] = "some sample data";
constexpr char kPlaintext[] = "plaintext";
}  // namespace

namespace google::scp::cpio::test {
shared_ptr<PutMetricsRequest> CreatePutMetricsRequest() {
  auto request = make_shared<PutMetricsRequest>();
  request->set_metric_namespace("test");
  auto metric = request->add_metrics();
  metric->set_name("test_metric");
  metric->set_value("12");
  metric->set_unit(
      cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_COUNT);

  auto labels = metric->mutable_labels();
  labels->insert(MapPair(string("lable_key"), string("label_value")));

  return request;
}

class CpioIntegrationTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    // Starts localstack
    if (StartLocalStackContainer("", kLocalstackContainerName,
                                 kLocalstackPort) != 0) {
      throw runtime_error("Failed to start localstack!");
    }
  }

  static void TearDownTestSuite() { StopContainer(kLocalstackContainerName); }

  void SetUp() override {
    cpio_options.log_option = LogOption::kConsoleLog;
    cpio_options.region = "us-east-1";
    cpio_options.owner_id = "123456789";
    cpio_options.instance_id = "987654321";
    cpio_options.sts_endpoint_override = localstack_endpoint;
    EXPECT_SUCCESS(TestLibCpio::InitCpio(cpio_options));
  }

  void TearDown() override {
    if (metric_client) {
      EXPECT_SUCCESS(metric_client->Stop());
    }
    if (parameter_client) {
      EXPECT_SUCCESS(parameter_client->Stop());
    }
    if (blob_storage_client) {
      EXPECT_SUCCESS(blob_storage_client->Stop());
    }
    if (kms_client) {
      EXPECT_SUCCESS(kms_client->Stop());
    }

    EXPECT_SUCCESS(TestLibCpio::ShutdownCpio(cpio_options));
  }

  void CreateMetricClient() {
    auto metric_client_options = make_shared<TestAwsMetricClientOptions>();
    metric_client_options->cloud_watch_endpoint_override =
        make_shared<string>(localstack_endpoint);
    metric_client = make_unique<TestAwsMetricClient>(metric_client_options);

    EXPECT_SUCCESS(metric_client->Init());
    EXPECT_SUCCESS(metric_client->Run());
  }

  void CreateParameterClientAndSetupData() {
    // Setup test data.
    auto ssm_client = CreateSSMClient(localstack_endpoint);
    PutParameter(ssm_client, kParameterName, kParameterValue);

    bool parameter_available = false;
    int8_t retry_count = 0;
    while (!parameter_available && retry_count < 20) {
      parameter_available = !GetParameter(ssm_client, kParameterName).empty();
      sleep_for(milliseconds(500));
      ++retry_count;
    }

    auto parameter_client_options =
        make_shared<TestAwsParameterClientOptions>();
    parameter_client_options->ssm_endpoint_override =
        make_shared<string>(localstack_endpoint);
    parameter_client =
        make_unique<TestAwsParameterClient>(parameter_client_options);

    EXPECT_SUCCESS(parameter_client->Init());
    EXPECT_SUCCESS(parameter_client->Run());
  }

  void CreateBlobStorageClientAndSetupData() {
    // Setup test data.
    auto s3_client = CreateS3Client(localstack_endpoint);
    CreateBucket(s3_client, kBucketName);

    auto blob_storage_client_options =
        make_shared<TestAwsBlobStorageClientOptions>();
    blob_storage_client_options->s3_endpoint_override =
        make_shared<string>(localstack_endpoint);
    blob_storage_client =
        make_unique<TestAwsBlobStorageClient>(blob_storage_client_options);

    EXPECT_SUCCESS(blob_storage_client->Init());
    EXPECT_SUCCESS(blob_storage_client->Run());
  }

  void CreateKmsClientAndSetupData(string& key_resource_name,
                                   string& ciphertext) {
    // Setup test data.
    auto aws_kms_client = CreateKMSClient(localstack_endpoint);
    string key_id;
    CreateKey(aws_kms_client, key_id, key_resource_name);
    string raw_ciphertext = Encrypt(aws_kms_client, key_id, kPlaintext);

    // KmsClient takes in encoded text.
    Base64Encode(raw_ciphertext, ciphertext);

    auto kms_client_options = make_shared<TestAwsKmsClientOptions>();
    kms_client_options->kms_endpoint_override =
        make_shared<string>(localstack_endpoint);
    kms_client = make_unique<TestAwsKmsClient>(kms_client_options);

    EXPECT_SUCCESS(kms_client->Init());
    EXPECT_SUCCESS(kms_client->Run());
  }

  string localstack_endpoint =
      string(kLocalHost) + ":" + string(kLocalstackPort);
  unique_ptr<TestAwsMetricClient> metric_client;
  unique_ptr<TestAwsParameterClient> parameter_client;
  unique_ptr<TestAwsBlobStorageClient> blob_storage_client;
  unique_ptr<TestAwsKmsClient> kms_client;

  TestCpioOptions cpio_options;
};

TEST_F(CpioIntegrationTest, MetricClientPutMetricsSuccessfully) {
  CreateMetricClient();

  vector<thread> threads;
  for (auto i = 0; i < 2; ++i) {
    threads.push_back(thread([&]() {
      for (auto j = 0; j < 5; j++) {
        atomic<bool> finished = false;
        auto context = AsyncContext<PutMetricsRequest, PutMetricsResponse>(
            CreatePutMetricsRequest(),
            [&](AsyncContext<PutMetricsRequest, PutMetricsResponse> context) {
              EXPECT_SUCCESS(context.result);
              finished = true;
            });
        EXPECT_SUCCESS(metric_client->PutMetrics(context));
        WaitUntil([&finished]() { return finished.load(); },
                  std::chrono::seconds(60));
      }
    }));
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

// GetInstanceId and GetTag cannot be tested in Localstack.
TEST_F(CpioIntegrationTest, ParameterClientGetParameterSuccessfully) {
  CreateParameterClientAndSetupData();

  atomic<bool> finished = false;
  GetParameterRequest request;
  request.set_parameter_name(kParameterName);
  EXPECT_EQ(
      parameter_client->GetParameter(
          std::move(request),
          [&](const ExecutionResult result, GetParameterResponse response) {
            EXPECT_SUCCESS(result);
            EXPECT_EQ(response.parameter_value(), kParameterValue);
            finished = true;
          }),
      SuccessExecutionResult());
  WaitUntil([&finished]() { return finished.load(); },
            std::chrono::seconds(60));
}

TEST_F(CpioIntegrationTest, BlobStorageClientPutBlobSuccessfully) {
  CreateBlobStorageClientAndSetupData();

  atomic<bool> finished = false;
  auto request = make_shared<PutBlobRequest>();
  request->mutable_blob()->mutable_metadata()->set_bucket_name(kBucketName);
  request->mutable_blob()->mutable_metadata()->set_blob_name(kBlobName);
  request->mutable_blob()->set_data(kBlobData);

  auto put_blob_context = AsyncContext<PutBlobRequest, PutBlobResponse>(
      move(request), [&](auto& context) {
        EXPECT_SUCCESS(context.result);
        finished = true;
      });

  EXPECT_SUCCESS(blob_storage_client->PutBlob(put_blob_context));
  WaitUntil([&finished]() { return finished.load(); },
            std::chrono::seconds(60));
}

TEST_F(CpioIntegrationTest, KmsClientDecryptSuccessfully) {
  string key_resource_name;
  string ciphertext;
  CreateKmsClientAndSetupData(key_resource_name, ciphertext);

  atomic<bool> finished = false;
  auto request = make_shared<DecryptRequest>();
  request->set_ciphertext(ciphertext);
  request->set_kms_region("us-east-1");
  request->set_key_resource_name(key_resource_name);
  // Set a fake identity. Localstack has no authentication check.
  request->set_account_identity("arn:aws:iam::123456:role/test_create_key");

  auto decrypt_context = AsyncContext<DecryptRequest, DecryptResponse>(
      move(request), [&](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->plaintext(), kPlaintext);
        finished = true;
      });

  EXPECT_SUCCESS(kms_client->Decrypt(decrypt_context));
  WaitUntil([&finished]() { return finished.load(); },
            std::chrono::seconds(60));
}
}  // namespace google::scp::cpio::test
