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

#include "cc/core/async_executor/mock/mock_async_executor.h"
#include "cc/core/http2_client/mock/mock_http_connection.h"
#include "cc/core/http2_client/mock/mock_http_connection_pool_with_overrides.h"
#include "cc/core/http2_client/src/error_codes.h"
#include "cc/core/interface/async_executor_interface.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::http2_client::mock::MockHttpConnection;
using google::scp::core::http2_client::mock::MockHttpConnectionPool;

namespace google::scp::core {

class HttpConnectionPoolTest : public testing::Test {
 protected:
  void SetUp() override {
    async_executor_ = std::make_shared<MockAsyncExecutor>();
    connection_pool_ = std::make_unique<MockHttpConnectionPool>(
        async_executor_, num_connections_per_host_);

    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(connection_pool_->Init());

    EXPECT_SUCCESS(async_executor_->Run());
    EXPECT_SUCCESS(connection_pool_->Run());
  }

  void TearDown() override {
    EXPECT_SUCCESS(connection_pool_->Stop());
    EXPECT_SUCCESS(async_executor_->Stop());
  }

  std::shared_ptr<AsyncExecutorInterface> async_executor_;
  std::unique_ptr<MockHttpConnectionPool> connection_pool_;
  size_t num_connections_per_host_ = 10;
};

TEST_F(HttpConnectionPoolTest, GetConnectionCreatesConnectionsForTheFirstTime) {
  auto uri = std::make_shared<Uri>("https://www.google.com:80");
  std::shared_ptr<HttpConnection> connection;
  EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection));

  auto map = connection_pool_->GetConnectionsMap();
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map["www.google.com:80"].size(), num_connections_per_host_);
}

TEST_F(HttpConnectionPoolTest, GetConnectionMultipleTimesDoesntRecreatePool) {
  std::vector<std::shared_ptr<HttpConnection>> connections_1;
  std::vector<std::shared_ptr<HttpConnection>> connections_2;
  {
    auto uri = std::make_shared<Uri>("https://www.google.com:80");
    std::shared_ptr<HttpConnection> connection;
    EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection));

    auto map = connection_pool_->GetConnectionsMap();
    EXPECT_EQ(map.size(), 1);
    EXPECT_EQ(map["www.google.com:80"].size(), num_connections_per_host_);
    for (auto& connection : map["www.google.com:80"]) {
      connections_1.push_back(connection);
    }
  }
  {
    auto uri = std::make_shared<Uri>("https://www.google.com:80");
    std::shared_ptr<HttpConnection> connection;
    EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection));

    auto map = connection_pool_->GetConnectionsMap();
    EXPECT_EQ(map.size(), 1);
    EXPECT_EQ(map["www.google.com:80"].size(), num_connections_per_host_);
    for (auto& connection : map["www.google.com:80"]) {
      connections_2.push_back(connection);
    }
  }
  EXPECT_EQ(connections_1, connections_2);
}

TEST_F(HttpConnectionPoolTest,
       GetConnectionCreatesConnectionPoolsForDifferentUris) {
  auto uri1 = std::make_shared<Uri>("https://www.google.com:80");
  auto uri2 = std::make_shared<Uri>("https://www.microsoft.com:80");

  std::shared_ptr<HttpConnection> connection1;
  EXPECT_SUCCESS(connection_pool_->GetConnection(uri1, connection1));

  std::shared_ptr<HttpConnection> connection2;
  EXPECT_SUCCESS(connection_pool_->GetConnection(uri2, connection2));

  auto map = connection_pool_->GetConnectionsMap();
  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map["www.google.com:80"].size(), num_connections_per_host_);
  EXPECT_EQ(map["www.microsoft.com:80"].size(), num_connections_per_host_);

  EXPECT_NE(connection1, connection2);
}

TEST_F(HttpConnectionPoolTest,
       GetConnectionMultipleTimesReturnsRoundRobinedConnectionsFromPool) {
  auto uri = std::make_shared<Uri>("https://www.google.com:80");
  std::shared_ptr<HttpConnection> connection;
  EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection));

  auto map = connection_pool_->GetConnectionsMap();
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map["www.google.com:80"].size(), num_connections_per_host_);

  std::vector<std::shared_ptr<HttpConnection>> connections_1;
  std::vector<std::shared_ptr<HttpConnection>> connections_2;

  connections_1.push_back(connection);

  for (int i = 0; i < num_connections_per_host_ - 1; i++) {
    std::shared_ptr<HttpConnection> connection;
    EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection));
    connections_1.push_back(connection);
  }

  for (int i = 0; i < num_connections_per_host_; i++) {
    std::shared_ptr<HttpConnection> connection;
    EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection));
    connections_2.push_back(connection);
  }

  EXPECT_EQ(connections_1, connections_2);
}

TEST_F(HttpConnectionPoolTest,
       GetConnectionOnADroppedConnectionRecyclesConnection) {
  std::atomic<size_t> create_connection_counter(0);
  std::atomic<bool> recycle_invoked_on_connection(false);
  // Every other connection is dropped.
  connection_pool_->create_connection_override_ =
      [&, async_executor = async_executor_](
          std::string host, std::string service, bool is_https) {
        auto connection = std::make_shared<MockHttpConnection>(
            async_executor, host, service, is_https);
        if (create_connection_counter % 2 != 0) {
          std::cout << "Dropping connection at " << create_connection_counter
                    << std::endl;
          connection->SetIsDropped();
          connection->SetIsNotReady();
          // Set the override for recycle connection to check if this Dropped
          // connection is invoked for recycling
          if (!connection_pool_->recycle_connection_override_) {
            connection_pool_->recycle_connection_override_ =
                [connection, &recycle_invoked_on_connection](
                    std::shared_ptr<HttpConnection>& connection_to_recycle) {
                  if (connection == connection_to_recycle) {
                    recycle_invoked_on_connection = true;
                  }
                };
          }
        } else {
          std::cout << "Connection at " << create_connection_counter
                    << " is ready" << std::endl;
          connection->SetIsNotDropped();
          connection->SetIsReady();
        }
        create_connection_counter++;
        std::shared_ptr<HttpConnection> connection_ptr = connection;
        return connection_ptr;
      };

  auto uri = std::make_shared<Uri>("https://www.google.com:80");
  std::shared_ptr<HttpConnection> connection1;
  EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection1));

  auto map = connection_pool_->GetConnectionsMap();
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map["www.google.com:80"].size(), num_connections_per_host_);

  auto connections = map["www.google.com:80"];

  EXPECT_EQ(connection1, connections[0]);

  std::shared_ptr<HttpConnection> connection2;
  EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection2));

  EXPECT_EQ(connection2, connections[2]);

  test::WaitUntil([&]() { return recycle_invoked_on_connection.load(); });
}

TEST_F(HttpConnectionPoolTest,
       GetConnectionOnNotReadyOneReturnsNextReadyConnectionInTheList) {
  std::atomic<size_t> create_connection_counter(0);
  // Every other connection is dropped.
  connection_pool_->create_connection_override_ =
      [&, async_executor = async_executor_](
          std::string host, std::string service, bool is_https) {
        auto connection = std::make_shared<MockHttpConnection>(
            async_executor, host, service, is_https);
        if (create_connection_counter % 2 != 0) {
          std::cout << "Dropping connection at " << create_connection_counter
                    << " and the connection is not ready yet" << std::endl;
          connection->SetIsDropped();
          connection->SetIsNotReady();
        } else {
          std::cout << "Connection at " << create_connection_counter
                    << " is ready" << std::endl;
          connection->SetIsNotDropped();
          connection->SetIsReady();
        }
        create_connection_counter++;
        std::shared_ptr<HttpConnection> connection_ptr = connection;
        return connection_ptr;
      };

  auto uri = std::make_shared<Uri>("https://www.google.com:80");
  std::shared_ptr<HttpConnection> connection1;
  EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection1));

  auto map = connection_pool_->GetConnectionsMap();
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map["www.google.com:80"].size(), num_connections_per_host_);

  auto connections = map["www.google.com:80"];

  EXPECT_EQ(connection1, connections[0]);

  std::shared_ptr<HttpConnection> connection2;
  EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection2));

  EXPECT_EQ(connection2, connections[2]);
}

TEST_F(HttpConnectionPoolTest,
       GetConnectionOnNotReadyConnectionsListReturnsARetryError) {
  std::atomic<size_t> create_connection_counter(0);
  // Every other connection is dropped.
  connection_pool_->create_connection_override_ =
      [&, async_executor = async_executor_](
          std::string host, std::string service, bool is_https) {
        auto connection = std::make_shared<MockHttpConnection>(
            async_executor, host, service, is_https);
        if (create_connection_counter % 2 != 0) {
          std::cout << "Dropping connection at " << create_connection_counter
                    << " and the connection is not ready yet" << std::endl;
          connection->SetIsDropped();
          connection->SetIsNotReady();
        } else {
          std::cout << "Connection at " << create_connection_counter
                    << " is ready" << std::endl;
          connection->SetIsNotDropped();
          connection->SetIsNotReady();
        }
        create_connection_counter++;
        std::shared_ptr<HttpConnection> connection_ptr = connection;
        return connection_ptr;
      };

  auto uri = std::make_shared<Uri>("https://www.google.com:80");
  std::shared_ptr<HttpConnection> connection1;
  EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection1));

  auto map = connection_pool_->GetConnectionsMap();
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map["www.google.com:80"].size(), num_connections_per_host_);

  auto connections = map["www.google.com:80"];

  EXPECT_EQ(connection1, connections[0]);

  std::shared_ptr<HttpConnection> connection2;
  EXPECT_THAT(connection_pool_->GetConnection(uri, connection2),
              test::ResultIs(RetryExecutionResult(
                  errors::SC_HTTP2_CLIENT_HTTP_CONNECTION_NOT_READY)));

  EXPECT_EQ(connection2, connections[1]);
}

TEST_F(HttpConnectionPoolTest,
       GetConnectionReturnsFirstReadyConnectionAfterSearchingAllOthers) {
  std::atomic<size_t> create_connection_counter(0);
  // Every other connection is dropped.
  connection_pool_->create_connection_override_ =
      [&, async_executor = async_executor_](
          std::string host, std::string service, bool is_https) {
        auto connection = std::make_shared<MockHttpConnection>(
            async_executor, host, service, is_https);
        if (create_connection_counter == 0) {
          connection->SetIsNotDropped();
          connection->SetIsReady();
        } else if (create_connection_counter == 1) {
          connection->SetIsDropped();
          connection->SetIsNotReady();
        } else {
          connection->SetIsNotDropped();
          connection->SetIsNotReady();
        }
        create_connection_counter++;
        std::shared_ptr<HttpConnection> connection_ptr = connection;
        return connection_ptr;
      };

  auto uri = std::make_shared<Uri>("https://www.google.com:80");
  std::shared_ptr<HttpConnection> connection1;
  EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection1));

  auto map = connection_pool_->GetConnectionsMap();
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map["www.google.com:80"].size(), num_connections_per_host_);

  auto connections = map["www.google.com:80"];

  EXPECT_EQ(connection1, connections[0]);

  std::shared_ptr<HttpConnection> connection2;
  EXPECT_SUCCESS(connection_pool_->GetConnection(uri, connection2));

  EXPECT_EQ(connection2, connections[0]);
}

}  // namespace google::scp::core
