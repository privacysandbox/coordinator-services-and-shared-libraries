
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
#include "http_connection_pool.h"

#include <algorithm>
#include <csignal>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <nghttp2/asio_http2.h>
#include <nghttp2/asio_http2_client.h>

#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/interface/http_client_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"
#include "http_connection.h"

using boost::algorithm::to_lower;
using boost::system::error_code;
using google::scp::core::common::kZeroUuid;
using nghttp2::asio_http2::host_service_from_uri;
using std::lock_guard;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

static constexpr char kHttpsTag[] = "https";
static constexpr char kHttpTag[] = "http";
static constexpr char kHttpConnection[] = "HttpConnection";

namespace google::scp::core {
ExecutionResult HttpConnectionPool::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult HttpConnectionPool::Run() noexcept {
  is_running_ = true;
  return SuccessExecutionResult();
}

ExecutionResult HttpConnectionPool::Stop() noexcept {
  is_running_ = false;
  vector<string> keys;
  auto execution_result = connections_.Keys(keys);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  for (const auto& key : keys) {
    shared_ptr<HttpConnectionPoolEntry> entry;
    execution_result = connections_.Find(key, entry);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    for (auto connection : entry->http_connections) {
      execution_result = connection->Stop();
      if (!execution_result.Successful()) {
        return execution_result;
      }
    }
  }

  return SuccessExecutionResult();
}

shared_ptr<HttpConnection> HttpConnectionPool::CreateHttpConnection(
    string host, string service, bool is_https,
    TimeDuration http2_read_timeout_in_sec) {
  return make_shared<HttpConnection>(async_executor_, host, service, is_https,
                                     http2_read_timeout_in_sec_);
}

ExecutionResult HttpConnectionPool::GetConnection(
    const shared_ptr<Uri>& uri,
    shared_ptr<HttpConnection>& connection) noexcept {
  if (!is_running_) {
    return FailureExecutionResult(
        errors::SC_HTTP2_CLIENT_CONNECTION_POOL_IS_NOT_AVAILABLE);
  }

  error_code ec;
  string scheme;
  string host;
  string service;
  if (host_service_from_uri(ec, scheme, host, service, *uri)) {
    return FailureExecutionResult(errors::SC_HTTP2_CLIENT_INVALID_URI);
  }

  to_lower(scheme);
  // TODO: remove support of non-https
  bool is_https = false;
  if (scheme == kHttpsTag) {
    is_https = true;
  } else if (scheme == kHttpTag) {
    is_https = false;
  } else {
    return FailureExecutionResult(errors::SC_HTTP2_CLIENT_INVALID_URI);
  }

  auto http_connection_entry = make_shared<HttpConnectionPoolEntry>();
  auto pair = std::make_pair(host + ":" + service, http_connection_entry);
  if (connections_.Insert(pair, http_connection_entry).Successful()) {
    for (size_t i = 0; i < max_connections_per_host_; ++i) {
      auto http_connection = CreateHttpConnection(host, service, is_https,
                                                  http2_read_timeout_in_sec_);
      http_connection_entry->http_connections.push_back(http_connection);
      auto execution_result = http_connection->Init();

      if (!execution_result.Successful()) {
        // Stop the connections already created before.
        http_connection_entry->http_connections.pop_back();
        for (auto& http_connection : http_connection_entry->http_connections) {
          http_connection->Stop();
        }
        connections_.Erase(pair.first);
        return execution_result;
      }

      execution_result = http_connection->Run();
      if (!execution_result.Successful()) {
        // Stop the connections already created before.
        http_connection_entry->http_connections.pop_back();
        for (auto& http_connection : http_connection_entry->http_connections) {
          http_connection->Stop();
        }
        connections_.Erase(pair.first);
        return execution_result;
      }
      SCP_INFO(kHttpConnection, kZeroUuid,
               "Successfully initialized a connection %p for %s",
               http_connection.get(), pair.first.c_str());
    }
    http_connection_entry->is_initialized = true;
  }

  if (!http_connection_entry->is_initialized.load()) {
    return RetryExecutionResult(
        errors::SC_HTTP2_CLIENT_NO_CONNECTION_ESTABLISHED);
  }

  auto value = http_connection_entry->order_counter.fetch_add(1);
  auto connections_index = value % max_connections_per_host_;
  connection = http_connection_entry->http_connections.at(connections_index);

  if (connection->IsDropped()) {
    RecycleConnection(connection);
    // If the current connection is not ready, pick another connection that is
    // ready. This allows the caller's request execution attempt to not go
    // waste.
    if (!connection->IsReady()) {
      size_t cur_index = connections_index;
      for (int i = 0; i < http_connection_entry->http_connections.size(); i++) {
        auto http_connection =
            http_connection_entry->http_connections[cur_index];
        if (http_connection->IsReady()) {
          connection = http_connection;
          break;
        }
        cur_index = (cur_index + 1) % max_connections_per_host_;
      }
      // Return a retry if we are not able to pick a ready connection.
      if (!connection->IsReady()) {
        return RetryExecutionResult(
            errors::SC_HTTP2_CLIENT_HTTP_CONNECTION_NOT_READY);
      }
    }
  }

  return SuccessExecutionResult();
}

void HttpConnectionPool::RecycleConnection(
    std::shared_ptr<HttpConnection>& connection) noexcept {
  lock_guard lock(connection_lock_);

  if (!connection->IsDropped()) {
    return;
  }

  connection->Stop();
  connection->Reset();
  connection->Init();
  connection->Run();

  SCP_DEBUG(kHttpConnection, common::kZeroUuid,
            "Successfully recycled connection %p", connection.get());
}
}  // namespace google::scp::core
