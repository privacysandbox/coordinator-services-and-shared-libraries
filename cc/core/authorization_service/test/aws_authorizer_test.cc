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

#include "core/authorization_service/src/aws_authorizer.h"

#include <gtest/gtest.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include <nghttp2/asio_http2_server.h>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/authorization_service/mock/mock_authorization_service_with_overrides.h"
#include "core/authorization_service/src/error_codes.h"
#include "core/common/auto_expiry_concurrent_map/mock/mock_auto_expiry_concurrent_map.h"
#include "core/http2_client/mock/mock_http_client.h"
#include "core/http2_client/src/error_codes.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using namespace nghttp2::asio_http2;          // NOLINT
using namespace nghttp2::asio_http2::server;  // NOLINT

using google::scp::core::AsyncExecutor;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::authorization_service::mock::
    MockAuthorizationServiceWithOverrides;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::function;
using std::make_shared;
using std::move;
using std::promise;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::vector;

namespace google::scp::core {

static string Base64Encode(const string& raw_data) {
  size_t required_len = 0;
  if (EVP_EncodedLength(&required_len, raw_data.length()) == 0) {
    return string();
  }
  string out(required_len, '\0');

  size_t output_len = EVP_EncodeBlock(
      reinterpret_cast<uint8_t*>(out.data()),
      reinterpret_cast<const uint8_t*>(raw_data.data()), raw_data.length());
  if (output_len == 0) {
    return string();
  }
  out.pop_back();  // EVP_EncodeBlock writes trailing \0. pop it.
  return out;
}

class MockAuthServer {
 public:
  MockAuthServer(string address, string port)
      : address_(address), port_(port), running_(false) {}

  ~MockAuthServer() {
    if (running_) {
      server_.stop();
      server_.join();
    }
  }

  void Run() {
    server_.handle("/success", [](const request& req, const response& res) {
      res.write_head(200);
      res.end(R"({"authorized_domain":"foo.com"})");
    });

    server_.handle("/forbidden", [](const request& req, const response& res) {
      res.write_head(403);
      res.end("Forbidden");
    });

    server_.handle("/malformed", [](const request& req, const response& res) {
      res.write_head(200);
      res.end("blah blah nothing useful");
    });
    boost::system::error_code ec;
    server_.listen_and_serve(ec, address_, port_, true);
    running_ = true;
  }

  string Port() { return to_string(server_.ports()[0]); }

 private:
  http2 server_;
  string address_;
  string port_;
  bool running_;
};

using Context = AsyncContext<AuthorizationRequest, AuthorizationResponse>;

TEST(AwsAuthorizerTest, BasicHappyPath) {
  MockAuthServer server("localhost", "0");
  server.Run();

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<AsyncExecutor>(2, 100);
  auto http_client = make_shared<HttpClient>(async_executor);
  async_executor->Init();
  async_executor->Run();
  http_client->Init();
  http_client->Run();
  auto authorizer = make_shared<AwsAuthorizer>(
      "http://localhost:" + server.Port() + "/success", "us-east-1",
      async_executor, http_client);

  auto request = make_shared<AuthorizationRequest>();
  request->authorization_token = make_shared<AuthorizationToken>(Base64Encode(
      R"({"access_key":"OHMYGOODLORD", "signature":"123456789abcdefabcdef", "amz_date":"19891107T123456Z"})"));
  request->claimed_identity = make_shared<string>("claimed_identity");

  promise<void> done;
  Context context(move(request), [&](Context& context) {
    EXPECT_SUCCESS(context.result);
    done.set_value();
  });
  ASSERT_EQ(authorizer->Authorize(context), SuccessExecutionResult());
  done.get_future().get();
  http_client->Stop();
  async_executor->Stop();
}

TEST(AwsAuthorizerTest, BasicUnauthorized) {
  MockAuthServer server("localhost", "0");
  server.Run();

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<AsyncExecutor>(2, 100);
  auto http_client = make_shared<HttpClient>(async_executor);
  async_executor->Init();
  async_executor->Run();
  http_client->Init();
  http_client->Run();
  auto authorizer = make_shared<AwsAuthorizer>(
      "http://localhost:" + server.Port() + "/forbidden", "us-east-1",
      async_executor, http_client);
  auto request = make_shared<AuthorizationRequest>();
  request->authorization_token = make_shared<AuthorizationToken>(Base64Encode(
      R"({"access_key":"OHMYGOODLORD", "signature":"123456789abcdefabcdef",
      "amz_date":"19891107T123456Z"})"));
  request->claimed_identity = make_shared<string>("claimed_identity");

  promise<void> done;
  Context context(move(request), [&](Context& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    errors::SC_HTTP2_CLIENT_HTTP_STATUS_FORBIDDEN)));
    done.set_value();
  });
  ASSERT_EQ(authorizer->Authorize(context), SuccessExecutionResult());
  done.get_future().get();
  http_client->Stop();
  async_executor->Stop();
}

TEST(AwsAuthorizerTest, MalformedServerResponse) {
  MockAuthServer server("localhost", "0");
  server.Run();

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<AsyncExecutor>(2, 100);
  auto http_client = make_shared<HttpClient>(async_executor);
  async_executor->Init();
  async_executor->Run();
  http_client->Init();
  http_client->Run();
  auto authorizer = make_shared<AwsAuthorizer>(
      "http://localhost:" + server.Port() + "/malformed", "us-east-1",
      async_executor, http_client);
  auto request = make_shared<AuthorizationRequest>();
  request->authorization_token = make_shared<AuthorizationToken>(Base64Encode(
      R"({"access_key":"OHMYGOODLORD", "signature":"123456789abcdefabcdef",
      "amz_date":"19891107T123456Z"})"));
  request->claimed_identity = make_shared<string>("claimed_identity");

  promise<void> done;
  Context context(move(request), [&](Context& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    errors::SC_AUTHORIZATION_SERVICE_INTERNAL_ERROR)));
    done.set_value();
  });
  ASSERT_EQ(authorizer->Authorize(context), SuccessExecutionResult());
  done.get_future().get();
  http_client->Stop();
  async_executor->Stop();
}

TEST(AwsAuthorizerTest, CannotConnectServer) {
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto http_client = make_shared<HttpClient>(async_executor);
  async_executor->Init();
  async_executor->Run();
  http_client->Init();
  http_client->Run();

  // Get a free port
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_GT(sock, 0);
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = 0;
  addr.sin_addr.s_addr = htonl(0x7F000001);
  int ret = ::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  EXPECT_EQ(ret, 0);
  socklen_t len = sizeof(addr);
  ret = getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len);
  EXPECT_EQ(ret, 0);
  EXPECT_GT(addr.sin_port, 0);
  string port = to_string(ntohs(addr.sin_port));

  auto authorizer = make_shared<AwsAuthorizer>(
      string("http://localhost:") + port + "/success", "us-east-1",
      async_executor, http_client);
  auto request = make_shared<AuthorizationRequest>();
  request->authorization_token = make_shared<AuthorizationToken>(Base64Encode(
      R"({"access_key":"OHMYGOODLORD", "signature":"123456789abcdefabcdef",
      "amz_date":"19891107T123456Z"})"));
  request->claimed_identity = make_shared<string>("claimed_identity");

  promise<void> done;
  Context context(move(request), [&](Context& context) {
    EXPECT_THAT(
        context.result,
        ResultIs(FailureExecutionResult(
            errors::SC_DISPATCHER_NOT_ENOUGH_TIME_REMAINED_FOR_OPERATION)));
    done.set_value();
  });

  ASSERT_EQ(authorizer->Authorize(context), SuccessExecutionResult());
  done.get_future().get();
  http_client->Stop();
  async_executor->Stop();
  close(sock);
}

TEST(AwsAuthorizerTest, MalformedToken) {
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto http_client = make_shared<HttpClient>(async_executor);
  async_executor->Init();
  async_executor->Run();
  http_client->Init();
  http_client->Run();
  auto authorizer =
      make_shared<AwsAuthorizer>("http://localhost:65534/success", "us-east-1",
                                 async_executor, http_client);
  auto request = make_shared<AuthorizationRequest>();
  request->authorization_token = make_shared<AuthorizationToken>(Base64Encode(
      R"({"bad_key":"OHMYGOODLORD", "signature":"123456789abcdefabcdef",
      "amz_date":"19891107T123456Z"})"));

  Context context(move(request), [&](Context& context) {});
  EXPECT_THAT(authorizer->Authorize(context),
              ResultIs(FailureExecutionResult(
                  errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN)));

  request->claimed_identity = make_shared<string>("claimed_identity");
  EXPECT_THAT(authorizer->Authorize(context),
              ResultIs(FailureExecutionResult(
                  errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN)));

  http_client->Stop();
  async_executor->Stop();
}

TEST(AwsAuthorizerTest, MalformedTokenBadEncoding) {
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto http_client = make_shared<HttpClient>(async_executor);
  async_executor->Init();
  async_executor->Run();
  http_client->Init();
  http_client->Run();
  auto authorizer =
      make_shared<AwsAuthorizer>("http://localhost:65534/success", "us-east-1",
                                 async_executor, http_client);
  auto request = make_shared<AuthorizationRequest>();
  request->authorization_token =
      make_shared<AuthorizationToken>("123321qwerfdaxcvdfasdf");

  Context context(move(request), [&](Context& context) {});
  EXPECT_THAT(authorizer->Authorize(context),
              ResultIs(FailureExecutionResult(
                  errors::SC_AUTHORIZATION_SERVICE_BAD_TOKEN)));
  http_client->Stop();
  async_executor->Stop();
}

TEST(AwsAuthorizerTest, BadConfig) {
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto http_client = make_shared<HttpClient>(async_executor);
  async_executor->Init();
  async_executor->Run();
  http_client->Init();
  http_client->Run();
  auto authorizer = make_shared<AwsAuthorizer>(
      "stp:/localhost:abcd/success", "us-east-1", async_executor, http_client);
  auto request = make_shared<AuthorizationRequest>();
  request->authorization_token = make_shared<AuthorizationToken>(Base64Encode(
      R"({"access_key":"OHMYGOODLORD", "signature":"123456789abcdefabcdef",
      "amz_date":"19891107T123456Z"})"));
  request->claimed_identity = make_shared<string>("claimed_identity");

  Context context(move(request), [&](Context& context) {});
  EXPECT_THAT(authorizer->Authorize(context),
              ResultIs(FailureExecutionResult(
                  errors::SC_AUTHORIZATION_SERVICE_INVALID_CONFIG)));
  http_client->Stop();
  async_executor->Stop();
}

TEST(AwsAuthorizerTest, HttpClientIsCalledOnlyOnceForEvaluatingTokens) {
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto http_client = make_shared<MockHttpClient>();

  auto authorizer = make_shared<MockAuthorizationServiceWithOverrides>(
      "http://localhost:441/success", "us-east-1", async_executor, http_client);

  auto request = make_shared<AuthorizationRequest>();
  request->authorization_token = make_shared<AuthorizationToken>(Base64Encode(
      R"({"access_key":"OHMYGOODLORD", "signature":"123456789abcdefabcdef",
      "amz_date":"19891107T123456Z"})"));
  request->claimed_identity = make_shared<string>("claimed_identity");

  AsyncContext<HttpRequest, HttpResponse> http_context;

  Context context(move(request), [&](Context& context) {});

  atomic<size_t> counter = 0;
  http_client->perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>
              http_context_callback) mutable {
        for (int i = 0; i < 100; ++i) {
          EXPECT_THAT(
              authorizer->Authorize(context),
              ResultIs(RetryExecutionResult(
                  errors::SC_AUTHORIZATION_SERVICE_AUTH_TOKEN_IS_REFRESHING)));
          vector<string> keys;
          authorizer->GetAuthorizationTokensMap()->Keys(keys);
          EXPECT_EQ(keys.size(), 1);
          EXPECT_EQ(
              authorizer->GetAuthorizationTokensMap()->IsEvictable(keys[0]),
              false);
        }

        http_context = http_context_callback;
        counter++;
        return SuccessExecutionResult();
      };

  EXPECT_SUCCESS(authorizer->Authorize(context));

  for (int i = 0; i < 100; ++i) {
    EXPECT_THAT(
        authorizer->Authorize(context),
        ResultIs(RetryExecutionResult(
            errors::SC_AUTHORIZATION_SERVICE_AUTH_TOKEN_IS_REFRESHING)));
  }

  http_context.response = make_shared<HttpResponse>();
  string body("{ \"authorized_domain\": \"blahblah\" }");
  http_context.response->body.bytes =
      make_shared<vector<Byte>>(body.begin(), body.end());
  http_context.response->body.length = body.length();
  http_context.result = SuccessExecutionResult();
  http_context.Finish();

  atomic<bool> called = false;
  context.callback = [&](Context& context) {
    EXPECT_EQ(*context.response->authorized_domain, "blahblah");
    called = true;
  };

  EXPECT_SUCCESS(authorizer->Authorize(context));
  EXPECT_EQ(counter.load(), 1);
  WaitUntil([&]() { return called.load(); });

  vector<string> keys;
  authorizer->GetAuthorizationTokensMap()->Keys(keys);
  EXPECT_EQ(keys.size(), 1);
  EXPECT_EQ(authorizer->GetAuthorizationTokensMap()->IsEvictable(keys[0]),
            true);
}

TEST(AwsAuthorizerTest, HttpClientFailureWillInvalidateCache) {
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto http_client = make_shared<MockHttpClient>();

  auto authorizer = make_shared<AwsAuthorizer>(
      "http://localhost:441/success", "us-east-1", async_executor, http_client);

  auto request = make_shared<AuthorizationRequest>();
  request->authorization_token = make_shared<AuthorizationToken>(Base64Encode(
      R"({"access_key":"OHMYGOODLORD", "signature":"123456789abcdefabcdef",
      "amz_date":"19891107T123456Z"})"));
  request->claimed_identity = make_shared<string>("claimed_identity");

  AsyncContext<HttpRequest, HttpResponse> http_context;

  Context context(move(request), [&](Context& context) {});

  atomic<size_t> counter = 0;
  http_client->perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>
              http_context_callback) mutable {
        for (int i = 0; i < 100; ++i) {
          EXPECT_THAT(
              authorizer->Authorize(context),
              ResultIs(RetryExecutionResult(
                  errors::SC_AUTHORIZATION_SERVICE_AUTH_TOKEN_IS_REFRESHING)));
        }

        EXPECT_EQ(
            http_context_callback.request->headers->find(kClaimedIdentityHeader)
                ->second,
            "claimed_identity");

        http_context = http_context_callback;
        counter++;
        return FailureExecutionResult(1234);
      };

  EXPECT_THAT(authorizer->Authorize(context),
              ResultIs(FailureExecutionResult(1234)));
  EXPECT_THAT(authorizer->Authorize(context),
              ResultIs(FailureExecutionResult(1234)));
  EXPECT_EQ(counter.load(), 2);
}

TEST(AwsAuthorizerTest, HttpClientFailureOnResponse) {
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto http_client = make_shared<MockHttpClient>();

  auto authorizer = make_shared<AwsAuthorizer>(
      "http://localhost:441/success", "us-east-1", async_executor, http_client);

  auto request = make_shared<AuthorizationRequest>();
  request->authorization_token = make_shared<AuthorizationToken>(Base64Encode(
      R"({"access_key":"OHMYGOODLORD", "signature":"123456789abcdefabcdef",
      "amz_date":"19891107T123456Z"})"));
  request->claimed_identity = make_shared<string>("claimed_identity");

  Context context(move(request), [&](Context& context) {});

  atomic<size_t> counter = 0;
  http_client->perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse> http_context) mutable {
        counter++;
        http_context.result = FailureExecutionResult(123);
        http_context.Finish();
        return SuccessExecutionResult();
      };

  EXPECT_SUCCESS(authorizer->Authorize(context));
  EXPECT_SUCCESS(authorizer->Authorize(context));
  EXPECT_EQ(counter.load(), 2);
}

TEST(AwsAuthorizerTest, OnBeforeGarbageCollection) {
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<MockAsyncExecutor>();
  auto http_client = make_shared<MockHttpClient>();

  auto authorizer = make_shared<MockAuthorizationServiceWithOverrides>(
      "http://localhost:441/success", "us-east-1", async_executor, http_client);

  bool called = false;
  string cache_entry_key;
  function<void(bool)> should_delete_entry = [&](bool should_delete) {
    EXPECT_EQ(should_delete, true);
    called = true;
  };
  authorizer->OnBeforeGarbageCollection(cache_entry_key, should_delete_entry);
  EXPECT_EQ(called, true);
}  // namespace google::scp::core

TEST(AwsAuthorizerTest, InsertionWhileDeletionShouldReturnRefreshStatusCode) {
  MockAuthServer server("localhost", "0");
  server.Run();

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<AsyncExecutor>(2, 100);
  auto http_client = make_shared<HttpClient>(async_executor);
  async_executor->Init();
  async_executor->Run();
  http_client->Init();
  http_client->Run();
  auto authorizer = make_shared<MockAuthorizationServiceWithOverrides>(
      "http://localhost:" + server.Port() + "/success", "us-east-1",
      async_executor, http_client);

  auto request = make_shared<AuthorizationRequest>();
  request->authorization_token = make_shared<AuthorizationToken>(Base64Encode(
      R"({"access_key":"OHMYGOODLORD", "signature":"123456789abcdefabcdef", "amz_date":"19891107T123456Z"})"));
  request->claimed_identity = make_shared<string>("claimed_identity");

  promise<void> done;
  Context context(request, [&](Context& context) {
    EXPECT_SUCCESS(context.result);
    done.set_value();
  });
  ASSERT_EQ(authorizer->Authorize(context), SuccessExecutionResult());
  done.get_future().get();

  vector<string> keys;
  authorizer->GetAuthorizationTokensMap()->Keys(keys);

  EXPECT_EQ(keys.size(), 1);
  authorizer->GetAuthorizationTokensMap()->insert_mock = []() {
    return FailureExecutionResult(
        errors::SC_AUTHORIZATION_SERVICE_ACCESS_DENIED);
  };

  ASSERT_EQ(
      authorizer->Authorize(context),
      FailureExecutionResult(errors::SC_AUTHORIZATION_SERVICE_ACCESS_DENIED));

  authorizer->GetAuthorizationTokensMap()->insert_mock = []() {
    return FailureExecutionResult(
        errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED);
  };

  ASSERT_EQ(authorizer->Authorize(context),
            RetryExecutionResult(
                errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED));

  http_client->Stop();
  async_executor->Stop();
}
}  // namespace google::scp::core
