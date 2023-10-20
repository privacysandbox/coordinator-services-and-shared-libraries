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

#include "test_pbs_server_starter.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <aws/core/Aws.h>
#include <aws/dynamodb/model/CreateTableRequest.h>
#include <aws/dynamodb/model/UpdateItemRequest.h>

#include "absl/strings/str_format.h"
#include "core/interface/configuration_keys.h"
#include "core/test/utils/aws_helper/aws_helper.h"
#include "core/test/utils/docker_helper/docker_helper.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/aws_integration_test/test_configuration_keys.h"

using Aws::Map;
using Aws::String;
using Aws::DynamoDB::DynamoDBClient;
using Aws::DynamoDB::Model::AttributeDefinition;
using Aws::DynamoDB::Model::AttributeValue;
using Aws::DynamoDB::Model::KeySchemaElement;
using Aws::DynamoDB::Model::KeyType;
using Aws::DynamoDB::Model::ScalarAttributeType;
using Aws::DynamoDB::Model::UpdateItemRequest;
using google::scp::core::test::CreateBucket;
using google::scp::core::test::CreateDynamoDbClient;
using google::scp::core::test::CreateImage;
using google::scp::core::test::CreateNetwork;
using google::scp::core::test::CreateS3Client;
using google::scp::core::test::CreateTable;
using google::scp::core::test::LoadImage;
using google::scp::core::test::PortMapToSelf;
using google::scp::core::test::RemoveNetwork;
using google::scp::core::test::StartContainer;
using google::scp::core::test::StartLocalStackContainer;
using google::scp::core::test::StopContainer;
using std::map;
using std::shared_ptr;
using std::string;
using std::vector;

static constexpr char kLocalHost[] = "http://127.0.0.1";
static constexpr char kBudgetKeyAttributeName[] = "Budget_Key";
static constexpr char kTimeframeAttributeName[] = "Timeframe";

static constexpr char kPbsServerImageLocation[] =
    "cc/pbs/deploy/pbs_server/build_defs/pbs_container_aws.tar";
static constexpr char kPbsServerImageName[] =
    "bazel/cc/pbs/deploy/pbs_server/build_defs:pbs_container_aws";

namespace google::scp::pbs::test::e2e {
static vector<AttributeDefinition> BuildAttributesForBudgetKeyTable() {
  vector<AttributeDefinition> attributes(2);

  AttributeDefinition attribute;
  attribute.SetAttributeName(kBudgetKeyAttributeName);
  attribute.SetAttributeType(ScalarAttributeType::S);
  attributes.push_back(attribute);

  attribute.SetAttributeName(kTimeframeAttributeName);
  attribute.SetAttributeType(ScalarAttributeType::S);
  attributes.push_back(attribute);

  return attributes;
}

static vector<KeySchemaElement> BuildSchemaForBudgetKeyTable() {
  vector<KeySchemaElement> schemas(2);

  KeySchemaElement schema;
  schema.SetAttributeName(kBudgetKeyAttributeName);
  schema.SetKeyType(KeyType::HASH);
  schemas.push_back(schema);

  schema.SetAttributeName(kTimeframeAttributeName);
  schema.SetKeyType(KeyType::RANGE);
  schemas.push_back(schema);

  return schemas;
}

static vector<AttributeDefinition> BuildAttributesForPartitionLockTable() {
  vector<AttributeDefinition> attributes(1);

  AttributeDefinition attribute;
  attribute.SetAttributeName(kPBSPartitionLockTableLockIdKeyName);
  attribute.SetAttributeType(ScalarAttributeType::S);
  attributes.push_back(attribute);

  return attributes;
}

static vector<KeySchemaElement> BuildSchemaForPartitionLockTable() {
  vector<KeySchemaElement> schemas(1);

  KeySchemaElement schema;
  schema.SetAttributeName(kPBSPartitionLockTableLockIdKeyName);
  schema.SetKeyType(KeyType::HASH);
  schemas.push_back(schema);

  return schemas;
}

void CreateDefaultPartitionLockTableRow(
    const shared_ptr<DynamoDBClient>& dynamo_db_client,
    const string& table_name) {
  UpdateItemRequest update_item_request;
  Aws::DynamoDB::Model::AttributeValue key_value;
  key_value.SetS("0");
  update_item_request.SetTableName(String(table_name));
  update_item_request.AddKey(kPBSPartitionLockTableLockIdKeyName, key_value);
  update_item_request.SetUpdateExpression(
      "SET LeaseExpirationTimestamp = :new_attribute_0 , LeaseOwnerId = "
      ":new_attribute_1 , LeaseOwnerServiceEndpointAddress = :new_attribute_2");
  Map<String, AttributeValue> attribute_values;

  attribute_values.emplace(":new_attribute_0", "0");
  attribute_values.emplace(":new_attribute_1", "0");
  attribute_values.emplace(":new_attribute_2", "0");
  update_item_request.SetExpressionAttributeValues(attribute_values);
  dynamo_db_client->UpdateItem(update_item_request);
}

int TestPbsServerStarter::RunTwoPbsServers(
    const TestPbsConfig& pbs_config, bool setup_data,
    std::map<std::string, std::string> env_overrides) {
  if (setup_data) {
    // Sets up DynamoDB and S3 data.
    string localstack_endpoint =
        string(kLocalHost) + ":" + config_.localstack_port;

    auto dynamo_db_client = CreateDynamoDbClient(localstack_endpoint);

    auto attributes_budget_key_table = BuildAttributesForBudgetKeyTable();
    auto schemas_budget_key_table = BuildSchemaForBudgetKeyTable();
    CreateTable(dynamo_db_client, pbs_config.pbs1_budget_key_table_name,
                attributes_budget_key_table, schemas_budget_key_table);
    CreateTable(dynamo_db_client, pbs_config.pbs2_budget_key_table_name,
                attributes_budget_key_table, schemas_budget_key_table);

    auto attributes_partition_lock_table =
        BuildAttributesForPartitionLockTable();
    auto schemas_partition_lock_table = BuildSchemaForPartitionLockTable();
    CreateTable(dynamo_db_client, pbs_config.pbs1_partition_lock_table_name,
                attributes_partition_lock_table, schemas_partition_lock_table);
    CreateTable(dynamo_db_client, pbs_config.pbs2_partition_lock_table_name,
                attributes_partition_lock_table, schemas_partition_lock_table);

    CreateDefaultPartitionLockTableRow(
        dynamo_db_client, pbs_config.pbs1_partition_lock_table_name);
    CreateDefaultPartitionLockTableRow(
        dynamo_db_client, pbs_config.pbs2_partition_lock_table_name);

    auto s3_client = CreateS3Client(localstack_endpoint);
    CreateBucket(s3_client, pbs_config.pbs1_journal_bucket_name);
    CreateBucket(s3_client, pbs_config.pbs2_journal_bucket_name);
  }

  std::cout << "Loading PBS image" << std::endl;
  auto result = LoadImage(kPbsServerImageLocation);
  if (result != 0) return result;

  std::cout << "Starting PBS 1" << std::endl;
  result = RunPbsServer1(pbs_config, env_overrides);
  if (result != 0) return result;

  std::cout << "Starting PBS 2" << std::endl;
  return RunPbsServer2(pbs_config, env_overrides);
}

int TestPbsServerStarter::RunPbsServer1(
    const TestPbsConfig& pbs_config,
    std::map<std::string, std::string> env_overrides) {
  auto env = CreatePbsEnvVariables(
      pbs_config.pbs1_budget_key_table_name,
      pbs_config.pbs1_partition_lock_table_name,
      pbs_config.pbs1_journal_bucket_name, pbs_config.pbs1_port,
      pbs_config.pbs1_health_port, "http://" + pbs_config.pbs2_container_name,
      pbs_config.pbs2_port);
  for (auto& env_override : env_overrides) {
    env[env_override.first] = env_override.second;
  }
  return StartContainer(config_.network_name, pbs_config.pbs1_container_name,
                        kPbsServerImageName,
                        PortMapToSelf(pbs_config.pbs1_port),
                        PortMapToSelf(pbs_config.pbs1_health_port), env);
}

int TestPbsServerStarter::RunPbsServer2(
    const TestPbsConfig& pbs_config,
    std::map<std::string, std::string> env_overrides) {
  auto env = CreatePbsEnvVariables(
      pbs_config.pbs2_budget_key_table_name,
      pbs_config.pbs2_partition_lock_table_name,
      pbs_config.pbs2_journal_bucket_name, pbs_config.pbs2_port,
      pbs_config.pbs2_health_port, "http://" + pbs_config.pbs1_container_name,
      pbs_config.pbs1_port);
  for (const auto& env_override : env_overrides) {
    env[env_override.first] = env_override.second;
  }
  return StartContainer(config_.network_name, pbs_config.pbs2_container_name,
                        kPbsServerImageName,
                        PortMapToSelf(pbs_config.pbs2_port),
                        PortMapToSelf(pbs_config.pbs2_health_port), env);
}

int TestPbsServerStarter::Setup() {
  // Creates network
  auto result = CreateNetwork(config_.network_name);
  if (result != 0) return result;

  // Starts localstack
  return StartLocalStackContainer(config_.network_name,
                                  config_.localstack_container_name,
                                  config_.localstack_port);
}

void TestPbsServerStarter::StopTwoPbsServers(const TestPbsConfig& pbs_config) {
  StopPbsServer1(pbs_config);
  StopPbsServer2(pbs_config);
}

void TestPbsServerStarter::StopPbsServer1(const TestPbsConfig& pbs_config) {
  StopContainer(pbs_config.pbs1_container_name);
}

void TestPbsServerStarter::StopPbsServer2(const TestPbsConfig& pbs_config) {
  StopContainer(pbs_config.pbs2_container_name);
}

void TestPbsServerStarter::Teardown() {
  StopContainer(config_.localstack_container_name);
  RemoveNetwork(config_.network_name);
}

map<string, string> TestPbsServerStarter::CreatePbsEnvVariables(
    const string& budget_key_table, const string& partition_lock_table,
    const string& journal_bucket, const string& host_port,
    const string& health_port, const string& remote_host,
    const string& remote_host_port) {
  map<string, string> env_variables;
  string localstack_endpoint_in_container = "http://" +
                                            config_.localstack_container_name +
                                            ":" + config_.localstack_port;

  // Useless dummy endpoint.
  string dummy_auth_server_endpoint = "http://dummy.auth.com";
  env_variables[kS3EndpointOverride] = localstack_endpoint_in_container;
  env_variables[kDynamoDbEndpointOverride] = localstack_endpoint_in_container;
  env_variables[kCloudwatchEndpointOverride] = localstack_endpoint_in_container;
  env_variables[kEC2MetadataEndpointOverride] =
      localstack_endpoint_in_container;
  env_variables[kPBSPartitionLockTableNameConfigName] = partition_lock_table;
  env_variables[core::kTransactionTimeoutInSecondsConfigName] =
      std::to_string(120);
  env_variables[kPBSPartitionLeaseDurationInSeconds] = "5";
  env_variables[core::kCloudServiceRegion] = config_.region;
  env_variables[kBudgetKeyTableName] = budget_key_table;
  env_variables[kJournalServiceBucketName] = journal_bucket;
  env_variables[kPrivacyBudgetServiceHostPort] = host_port;
  env_variables[kPrivacyBudgetServiceHealthPort] = health_port;
  env_variables[kAuthServiceEndpoint] = dummy_auth_server_endpoint;
  env_variables[kRemotePrivacyBudgetServiceHostAddress] =
      remote_host + ":" + remote_host_port;
  env_variables[kRemotePrivacyBudgetServiceCloudServiceRegion] = config_.region;
  env_variables[kRemotePrivacyBudgetServiceAuthServiceEndpoint] =
      dummy_auth_server_endpoint;
  env_variables[kRemotePrivacyBudgetServiceClaimedIdentity] =
      config_.reporting_origin;
  env_variables[kRemotePrivacyBudgetServiceAssumeRoleArn] = "arn";
  env_variables[kRemotePrivacyBudgetServiceAssumeRoleExternalId] =
      "external_id";

  env_variables[kAsyncExecutorQueueSize] = "1000";
  env_variables[kAsyncExecutorThreadsCount] = "2";
  env_variables[kIOAsyncExecutorQueueSize] = "1000";
  env_variables[kIOAsyncExecutorThreadsCount] = "2";
  env_variables[kTransactionManagerCapacity] = "1000";
  env_variables[kServiceMetricsNamespace] = "PBS";
  env_variables[kTotalHttp2ServerThreadsCount] = "2";
  env_variables[kPBSPartitionLockTableNameConfigName] = "partition_lock_table";
  env_variables[kJournalServicePartitionName] =
      "00000000-0000-0000-0000-000000000000";
  env_variables[kPrivacyBudgetServiceHostAddress] = "0.0.0.0";
  env_variables[kPBSMultiInstanceModeDisabledConfigKey] = "true";
  return env_variables;
}
}  // namespace google::scp::pbs::test::e2e
