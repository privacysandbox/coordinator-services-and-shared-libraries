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

#include "aws_helper.h"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <aws/core/client/ClientConfiguration.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/CreateTableRequest.h>
#include <aws/kms/KMSClient.h>
#include <aws/kms/model/CreateKeyRequest.h>
#include <aws/kms/model/EncryptRequest.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/ssm/SSMClient.h>
#include <aws/ssm/model/GetParametersRequest.h>
#include <aws/ssm/model/PutParameterRequest.h>

using Aws::Client::ClientConfiguration;
using Aws::DynamoDB::DynamoDBClient;
using Aws::DynamoDB::DynamoDBErrors;
using Aws::DynamoDB::Model::AttributeDefinition;
using Aws::DynamoDB::Model::CreateTableOutcome;
using Aws::DynamoDB::Model::CreateTableRequest;
using Aws::DynamoDB::Model::KeySchemaElement;
using Aws::DynamoDB::Model::KeyType;
using Aws::DynamoDB::Model::ProvisionedThroughput;
using Aws::DynamoDB::Model::ScalarAttributeType;
using Aws::KMS::KMSClient;
using Aws::KMS::Model::CreateKeyRequest;
using Aws::KMS::Model::EncryptRequest;
using Aws::S3::S3Client;
using Aws::S3::Model::BucketCannedACL;
using Aws::S3::Model::CreateBucketRequest;
using Aws::SSM::SSMClient;
using Aws::SSM::Model::GetParametersRequest;
using Aws::SSM::Model::PutParameterRequest;
using Aws::Utils::CryptoBuffer;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

/// Fixed connect timeout to create an AWS client.
const int kConnectTimeoutMs = 6000;
/// Fixed request timeout to create an AWS client.
const int kRequestTimeoutMs = 12000;
/// Fixed read and write capacity for DynamoDB.
const int kReadWriteCapacity = 10;

namespace google::scp::core::test {
shared_ptr<ClientConfiguration> CreateClientConfiguration(
    const string& endpoint, const string& region) {
  auto config = make_shared<ClientConfiguration>();
  config->region = region;
  config->scheme = Aws::Http::Scheme::HTTP;
  config->connectTimeoutMs = kConnectTimeoutMs;
  config->requestTimeoutMs = kRequestTimeoutMs;
  config->endpointOverride = endpoint;
  return config;
}

shared_ptr<DynamoDBClient> CreateDynamoDbClient(const string& endpoint,
                                                const string& region) {
  return make_shared<DynamoDBClient>(
      *CreateClientConfiguration(endpoint, region));
}

void CreateTable(const shared_ptr<DynamoDBClient>& dynamo_db_client,
                 const string& table_name,
                 const vector<AttributeDefinition>& attributes,
                 const vector<KeySchemaElement>& schemas) {
  CreateTableRequest request;
  request.SetTableName(table_name.c_str());

  for (auto attribute : attributes) {
    request.AddAttributeDefinitions(attribute);
  }

  for (auto schema : schemas) {
    request.AddKeySchema(schema);
  }

  ProvisionedThroughput throughput;
  throughput.SetReadCapacityUnits(kReadWriteCapacity);
  throughput.SetWriteCapacityUnits(kReadWriteCapacity);
  request.SetProvisionedThroughput(throughput);

  auto outcome = dynamo_db_client->CreateTable(request);
  if (!outcome.IsSuccess()) {
    std::cout << "Failed to create table: " << outcome.GetError().GetMessage()
              << std::endl;
  } else {
    std::cout << "Succeeded to create table:" << table_name << std::endl;
  }
}

shared_ptr<S3Client> CreateS3Client(const string& endpoint,
                                    const string& region) {
  // Should disable virtual host, otherwise, our path-style url will not work.
  return make_shared<S3Client>(
      *CreateClientConfiguration(endpoint, region),
      Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
      /* use virtual host */ false);
}

void CreateBucket(const shared_ptr<S3Client>& s3_client,
                  const string& bucket_name) {
  CreateBucketRequest request;
  request.SetBucket(bucket_name.c_str());
  request.SetACL(BucketCannedACL::public_read_write);

  auto outcome = s3_client->CreateBucket(request);
  if (!outcome.IsSuccess()) {
    std::cout << "Failed to create bucket: " << outcome.GetError().GetMessage()
              << std::endl;
  } else {
    std::cout << "Succeeded to create bucket: " << bucket_name << std::endl;
  }
}

std::shared_ptr<SSMClient> CreateSSMClient(const std::string& endpoint,
                                           const std::string& region) {
  return make_shared<SSMClient>(*CreateClientConfiguration(endpoint, region));
}

void PutParameter(const std::shared_ptr<SSMClient>& ssm_client,
                  const std::string& parameter_name,
                  const std::string& parameter_value) {
  PutParameterRequest request;
  request.SetName(parameter_name.c_str());
  request.SetValue(parameter_value.c_str());

  auto outcome = ssm_client->PutParameter(request);
  if (!outcome.IsSuccess()) {
    std::cout << "Failed to put parameter: " << outcome.GetError().GetMessage()
              << std::endl;
  } else {
    std::cout << "Succeeded to put parameter:" << parameter_name << std::endl;
  }
}

std::string GetParameter(const std::shared_ptr<SSMClient>& ssm_client,
                         const std::string& parameter_name) {
  GetParametersRequest request;
  request.AddNames(parameter_name.c_str());

  auto outcome = ssm_client->GetParameters(request);
  if (!outcome.IsSuccess()) {
    std::cout << "Failed to get parameter: " << outcome.GetError().GetMessage()
              << std::endl;
    return "";
  } else {
    if (outcome.GetResult().GetParameters().size() != 1) {
      std::cout << "Parameter number does not match! The size is: "
                << outcome.GetResult().GetParameters().size() << std::endl;
      return "";
    }
    std::cout << "Succeeded to get parameter:" << parameter_name << std::endl;
    return outcome.GetResult().GetParameters()[0].GetValue();
  }
}

shared_ptr<KMSClient> CreateKMSClient(const string& endpoint,
                                      const string& region) {
  return make_shared<KMSClient>(*CreateClientConfiguration(endpoint, region));
}

void CreateKey(const shared_ptr<KMSClient>& kms_client, string& key_id,
               string& key_resource_name) {
  CreateKeyRequest request;

  // Needs to retry until succeeded.
  int8_t retry_count = 0;
  while (retry_count < 20) {
    auto outcome = kms_client->CreateKey(request);
    if (!outcome.IsSuccess()) {
      std::cout << "Failed to create key: " << outcome.GetError().GetMessage()
                << std::endl;
      sleep_for(milliseconds(500));
      ++retry_count;
    } else {
      std::cout << "Succeeded to create key." << std::endl;
      key_id = outcome.GetResult().GetKeyMetadata().GetKeyId();
      key_resource_name = outcome.GetResult().GetKeyMetadata().GetArn();
      return;
    }
  }
}

string Encrypt(const std::shared_ptr<KMSClient>& kms_client,
               const string& key_id, const string& plaintext) {
  EncryptRequest request;
  request.SetKeyId(key_id);
  Aws::Utils::ByteBuffer plaintext_buffer(
      reinterpret_cast<const unsigned char*>(plaintext.data()),
      plaintext.length());
  request.SetPlaintext(move(plaintext_buffer));
  auto outcome = kms_client->Encrypt(request);
  if (!outcome.IsSuccess()) {
    std::cout << "Failed to encrypt: " << outcome.GetError().GetMessage()
              << std::endl;
    return "";
  } else {
    std::cout << "Succeeded to encrypt: " << plaintext << std::endl;
    auto& blob = outcome.GetResult().GetCiphertextBlob();
    return string(reinterpret_cast<const char*>(blob.GetUnderlyingData()),
                  blob.GetLength());
  }
}
}  // namespace google::scp::core::test
