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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/CreateTableRequest.h>
#include <aws/s3/S3Client.h>
#include <aws/ssm/SSMClient.h>

namespace google::scp::core::test {
/// Default AWS region to create clients.
constexpr char kDefaultRegion[] = "us-east-1";

std::shared_ptr<Aws::DynamoDB::DynamoDBClient> CreateDynamoDbClient(
    const std::string& endpoint, const std::string& region = kDefaultRegion);

void CreateTable(
    const std::shared_ptr<Aws::DynamoDB::DynamoDBClient>& dynamo_db_client,
    const std::string& table_name,
    const std::vector<Aws::DynamoDB::Model::AttributeDefinition>& attributes,
    const std::vector<Aws::DynamoDB::Model::KeySchemaElement>& schemas);

std::shared_ptr<Aws::S3::S3Client> CreateS3Client(
    const std::string& endpoint, const std::string& region = kDefaultRegion);

void CreateBucket(const std::shared_ptr<Aws::S3::S3Client>& s3_client,
                  const std::string& bucket_name);

std::shared_ptr<Aws::SSM::SSMClient> CreateSSMClient(
    const std::string& endpoint, const std::string& region = kDefaultRegion);

void PutParameter(const std::shared_ptr<Aws::SSM::SSMClient>& ssm_client,
                  const std::string& parameter_name,
                  const std::string& parameter_value);
}  // namespace google::scp::core::test
