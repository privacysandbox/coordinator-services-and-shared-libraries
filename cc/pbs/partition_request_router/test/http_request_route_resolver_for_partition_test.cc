/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pbs/partition_request_router/src/http_request_route_resolver_for_partition.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/partition_namespace_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/type_def.h"
#include "pbs/partition/mock/pbs_partition_mock.h"
#include "pbs/partition_manager/mock/pbs_partition_manager_mock.h"
#include "pbs/partition_namespace/mock/partition_namespace_mock.h"
#include "pbs/partition_request_router/src/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpRequest;
using google::scp::core::PartitionId;
using google::scp::core::ResourceId;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TransactionManagerInterface;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::test::ResultIs;
using google::scp::pbs::partition::mock::MockPBSPartition;
using google::scp::pbs::partition_manager::mock::MockPBSPartitionManager;
using google::scp::pbs::partition_namespace::mock::MockPartitionNamespace;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using ::testing::_;
using ::testing::An;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::ReturnRef;

static constexpr char kDefaultRemoteCoordinatorClaimedIdentity[] = "";

namespace google::scp::pbs::test {
class HttpRequestRouteResolverForPartitionTest : public testing::Test {
 protected:
  HttpRequestRouteResolverForPartitionTest() {
    auto config_provider = make_shared<MockConfigProvider>();
    config_provider->Set(kRemotePrivacyBudgetServiceClaimedIdentity,
                         kDefaultRemoteCoordinatorClaimedIdentity);
    partition_manager_mock_ = make_shared<MockPBSPartitionManager>();
    partition_namespace_mock_ = make_shared<MockPartitionNamespace>();
    request_route_resolver_ = make_unique<HttpRequestRouteResolverForPartition>(
        partition_namespace_mock_, partition_manager_mock_, config_provider);
  }

  shared_ptr<MockPBSPartitionManager> partition_manager_mock_;
  shared_ptr<MockPartitionNamespace> partition_namespace_mock_;
  unique_ptr<HttpRequestRouteResolverForPartition> request_route_resolver_;
  PartitionId partition_id_ = {1, 2};
};

TEST_F(HttpRequestRouteResolverForPartitionTest, ResolveRouteRemoteEndpoint) {
  EXPECT_CALL(*partition_manager_mock_, GetPartitionAddress(partition_id_))
      .WillOnce(Return(make_shared<std::string>("https://1.1.1.1")));
  EXPECT_CALL(*partition_namespace_mock_, MapResourceToPartition)
      .WillOnce([=](const ResourceId& resource_id) {
        EXPECT_EQ(resource_id, "www.google.com");
        return partition_id_;
      });

  HttpRequest request;
  request.headers = make_shared<HttpHeaders>();
  request.headers->insert(
      {string(core::kClaimedIdentityHeader), "www.google.com"});

  auto result_or = request_route_resolver_->ResolveRoute(request);
  EXPECT_SUCCESS(result_or.result());
  EXPECT_EQ(*(*result_or).uri, "https://1.1.1.1");
  EXPECT_FALSE((*result_or).is_local_endpoint);
}

TEST_F(HttpRequestRouteResolverForPartitionTest,
       ResolveRouteLocalEndpointForOtherCoordinatorRequest) {
  EXPECT_CALL(*partition_manager_mock_, GetPartitionAddress(partition_id_))
      .WillOnce(Return(make_shared<std::string>("https://1.1.1.1")));
  EXPECT_CALL(*partition_namespace_mock_, MapResourceToPartition)
      .WillOnce([=](const ResourceId& resource_id) {
        EXPECT_EQ(resource_id, "www.google.com");
        return partition_id_;
      });

  HttpRequest request;
  request.headers = make_shared<HttpHeaders>();
  request.headers->insert({string(core::kClaimedIdentityHeader),
                           kDefaultRemoteCoordinatorClaimedIdentity});
  request.headers->insert({string(kTransactionOriginHeader), "www.google.com"});

  auto result_or = request_route_resolver_->ResolveRoute(request);
  EXPECT_SUCCESS(result_or.result());
  EXPECT_EQ(*(*result_or).uri, "https://1.1.1.1");
  EXPECT_FALSE((*result_or).is_local_endpoint);
}

TEST_F(HttpRequestRouteResolverForPartitionTest,
       ResolveRouteMissingRouteInformation) {
  HttpRequest request;
  request.headers = make_shared<HttpHeaders>();

  // If no headers are present
  {
    auto result_or = request_route_resolver_->ResolveRoute(request);
    EXPECT_THAT(result_or.result(),
                ResultIs(FailureExecutionResult(
                    core::errors::
                        SC_PBS_TRANSACTION_REQUEST_ROUTER_MISSING_ROUTING_ID)));
  }

  // If claimed identity header is not present
  request.headers->insert({string(kTransactionOriginHeader), "www.google.com"});
  {
    auto result_or = request_route_resolver_->ResolveRoute(request);
    EXPECT_THAT(result_or.result(),
                ResultIs(FailureExecutionResult(
                    core::errors::
                        SC_PBS_TRANSACTION_REQUEST_ROUTER_MISSING_ROUTING_ID)));
  }

  // If remote cordinator request but transaction origin header is not present.
  request.headers->clear();
  request.headers->insert({string(core::kClaimedIdentityHeader),
                           kDefaultRemoteCoordinatorClaimedIdentity});
  {
    auto result_or = request_route_resolver_->ResolveRoute(request);
    EXPECT_THAT(result_or.result(),
                ResultIs(FailureExecutionResult(
                    core::errors::
                        SC_PBS_TRANSACTION_REQUEST_ROUTER_MISSING_ROUTING_ID)));
  }
}

TEST_F(HttpRequestRouteResolverForPartitionTest, ResolveRouteLocalEndpoint) {
  EXPECT_CALL(*partition_manager_mock_, GetPartitionAddress(partition_id_))
      .WillOnce(Return(make_shared<string>(core::kLocalPartitionAddressUri)));
  EXPECT_CALL(*partition_namespace_mock_,
              MapResourceToPartition("www.google.com"))
      .WillOnce(Return(partition_id_));

  HttpRequest request;
  request.headers = make_shared<HttpHeaders>();
  request.headers->insert(
      {string(core::kClaimedIdentityHeader), "www.google.com"});

  auto result_or = request_route_resolver_->ResolveRoute(request);
  EXPECT_SUCCESS(result_or.result());
  EXPECT_EQ(*(*result_or).uri, core::kLocalPartitionAddressUri);
  EXPECT_TRUE((*result_or).is_local_endpoint);
}

TEST_F(HttpRequestRouteResolverForPartitionTest,
       ResolveRouteFailsDueToPartitionManager) {
  EXPECT_CALL(*partition_manager_mock_, GetPartitionAddress(partition_id_))
      .WillOnce(Return(FailureExecutionResult(1234)));
  EXPECT_CALL(*partition_namespace_mock_,
              MapResourceToPartition("www.google.com"))
      .WillOnce(Return(partition_id_));

  HttpRequest request;
  request.headers = make_shared<HttpHeaders>();
  request.headers->insert(
      {string(core::kClaimedIdentityHeader), "www.google.com"});

  auto result_or = request_route_resolver_->ResolveRoute(request);
  EXPECT_THAT(
      result_or,
      ResultIs(FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_REQUEST_ROUTER_PARTITION_UNAVAILABLE)));
}

}  // namespace google::scp::pbs::test
