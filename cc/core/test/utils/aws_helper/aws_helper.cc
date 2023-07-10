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

#include <memory>
#include <string>
#include <vector>

#include <aws/core/client/ClientConfiguration.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/CreateTableRequest.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/ssm/SSMClient.h>
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
using Aws::S3::S3Client;
using Aws::S3::Model::BucketCannedACL;
using Aws::S3::Model::CreateBucketRequest;
using Aws::SSM::SSMClient;
using Aws::SSM::Model::PutParameterRequest;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

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
}  // namespace google::scp::core::test
