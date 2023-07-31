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

#include "core/authorization_proxy/src/authorization_proxy.h"

#include <gtest/gtest.h>

#include <gmock/gmock.h>

#include "core/async_executor/src/async_executor.h"
#include "core/authorization_proxy/src/error_codes.h"
#include "core/interface/async_context.h"
#include "core/interface/http_request_response_auth_interceptor_interface.h"
#include "core/test/utils/conditional_wait.h"

using google::scp::core::AsyncExecutor;
using std::make_shared;
using std::shared_ptr;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace google::scp::core::test {

class HttpRequestResponseAuthInterceptorMock
    : public HttpRequestResponseAuthInterceptorInterface {
 public:
  MOCK_METHOD(ExecutionResult, AddHeadersToRequest,
              (const AuthorizationMetadata&, HttpRequest&), (override));
  MOCK_METHOD(ExecutionResultOr<AuthorizedMetadata>,
              ObtainAuthorizedMetadataFromResponse, (const HttpResponse&),
              (override));
};

class HttpClientMock : public HttpClientInterface {
 public:
  MOCK_METHOD(ExecutionResult, PerformRequest,
              ((AsyncContext<HttpRequest, HttpResponse>&)),
              (noexcept, override));
  MOCK_METHOD(ExecutionResult, Init, (), (noexcept, override));
  MOCK_METHOD(ExecutionResult, Run, (), (noexcept, override));
  MOCK_METHOD(ExecutionResult, Stop, (), (noexcept, override));
};

class AuthorizationProxyTest : public testing::Test {
 protected:
  AuthorizationProxyTest()
      : mock_http_client_(make_shared<HttpClientMock>()),
        async_executor_(
            make_shared<AsyncExecutor>(4, 1000, true /* drop tasks on stop */)),
        server_endpoint_("http://auth.google.com:8080/submit") {
    EXPECT_EQ(async_executor_->Init(), SuccessExecutionResult());
    EXPECT_EQ(async_executor_->Run(), SuccessExecutionResult());

    authorization_metadata_.claimed_identity = "google.com";
    authorization_metadata_.authorization_token = "kjgasuif8i2qr1kj215125";

    authorized_metadata_.authorized_domain =
        std::make_shared<std::string>("google.com");
  }

  void TearDown() override {
    EXPECT_EQ(async_executor_->Stop(), SuccessExecutionResult());
  }

  std::shared_ptr<HttpClientMock> mock_http_client_;
  std::shared_ptr<AsyncExecutorInterface> async_executor_;
  std::string server_endpoint_;
  AuthorizationMetadata authorization_metadata_;
  AuthorizedMetadata authorized_metadata_;
};

TEST_F(AuthorizationProxyTest, InvalidServiceEndpointURI) {
  auto invalid_server_endpoint_uri = "localhost:8080";
  std::unique_ptr<HttpRequestResponseAuthInterceptorInterface>
      authorization_http_helper =
          std::make_unique<HttpRequestResponseAuthInterceptorMock>();
  AuthorizationProxy proxy(invalid_server_endpoint_uri, async_executor_,
                           mock_http_client_,
                           std::move(authorization_http_helper));
  EXPECT_EQ(proxy.Init(), FailureExecutionResult(
                              errors::SC_AUTHORIZATION_PROXY_INVALID_CONFIG));
}

TEST_F(AuthorizationProxyTest, ValidServiceEndpointURI) {
  std::unique_ptr<HttpRequestResponseAuthInterceptorInterface>
      authorization_http_helper =
          std::make_unique<HttpRequestResponseAuthInterceptorMock>();
  AuthorizationProxy proxy(server_endpoint_, async_executor_, mock_http_client_,
                           std::move(authorization_http_helper));
  EXPECT_EQ(proxy.Init(), SuccessExecutionResult());
}

TEST_F(AuthorizationProxyTest, AuthorizeWithInvalidAuthorizationMetadata) {
  auto authorization_http_helper =
      std::make_unique<HttpRequestResponseAuthInterceptorMock>();

  AuthorizationProxy proxy(server_endpoint_, async_executor_, mock_http_client_,
                           std::move(authorization_http_helper));
  EXPECT_EQ(proxy.Init(), SuccessExecutionResult());
  EXPECT_EQ(proxy.Run(), SuccessExecutionResult());

  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_request1;
  authorization_request1.request = make_shared<AuthorizationProxyRequest>();
  EXPECT_EQ(proxy.Authorize(authorization_request1),
            FailureExecutionResult(errors::SC_AUTHORIZATION_PROXY_BAD_REQUEST));

  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_request2;
  authorization_request2.request = make_shared<AuthorizationProxyRequest>();
  authorization_request2.request->authorization_metadata.claimed_identity =
      "claimed_id";
  EXPECT_EQ(proxy.Authorize(authorization_request2),
            FailureExecutionResult(errors::SC_AUTHORIZATION_PROXY_BAD_REQUEST));

  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_request3;
  authorization_request3.request = make_shared<AuthorizationProxyRequest>();
  authorization_request3.request->authorization_metadata.authorization_token =
      "auth_token";
  EXPECT_EQ(proxy.Authorize(authorization_request3),
            FailureExecutionResult(errors::SC_AUTHORIZATION_PROXY_BAD_REQUEST));
}

TEST_F(AuthorizationProxyTest,
       AuthorizeReturnsFailureDueToInvalidHeaderFormation) {
  auto authorization_http_helper =
      std::make_unique<HttpRequestResponseAuthInterceptorMock>();

  HttpRequestResponseAuthInterceptorMock* authorization_http_helper_mock =
      authorization_http_helper.get();

  AuthorizationProxy proxy(server_endpoint_, async_executor_, mock_http_client_,
                           std::move(authorization_http_helper));
  EXPECT_EQ(proxy.Init(), SuccessExecutionResult());
  EXPECT_EQ(proxy.Run(), SuccessExecutionResult());

  EXPECT_CALL(*authorization_http_helper_mock, AddHeadersToRequest(_, _))
      .WillOnce(Return(FailureExecutionResult(123)));

  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_request;
  authorization_request.request = make_shared<AuthorizationProxyRequest>();
  authorization_request.request->authorization_metadata =
      authorization_metadata_;

  EXPECT_EQ(proxy.Authorize(authorization_request),
            FailureExecutionResult(errors::SC_AUTHORIZATION_PROXY_BAD_REQUEST));
}

TEST_F(AuthorizationProxyTest, AuthorizeReturnsRetryDueToRemoteError) {
  auto authorization_http_helper =
      std::make_unique<HttpRequestResponseAuthInterceptorMock>();

  HttpRequestResponseAuthInterceptorMock* authorization_http_helper_mock =
      authorization_http_helper.get();

  AuthorizationProxy proxy(server_endpoint_, async_executor_, mock_http_client_,
                           std::move(authorization_http_helper));
  EXPECT_EQ(proxy.Init(), SuccessExecutionResult());
  EXPECT_EQ(proxy.Run(), SuccessExecutionResult());

  EXPECT_CALL(*authorization_http_helper_mock, AddHeadersToRequest(_, _))
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(*mock_http_client_, PerformRequest)
      .WillOnce(Return(FailureExecutionResult(123)));

  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_request;
  authorization_request.request = make_shared<AuthorizationProxyRequest>();
  authorization_request.request->authorization_metadata =
      authorization_metadata_;

  EXPECT_EQ(
      proxy.Authorize(authorization_request),
      RetryExecutionResult(errors::SC_AUTHORIZATION_PROXY_REMOTE_UNAVAILABLE));
}

TEST_F(AuthorizationProxyTest,
       AuthorizeReturnsRetryDueToRemoteErrorAsCallback) {
  auto authorization_http_helper =
      std::make_unique<HttpRequestResponseAuthInterceptorMock>();

  HttpRequestResponseAuthInterceptorMock* authorization_http_helper_mock =
      authorization_http_helper.get();

  AuthorizationProxy proxy(server_endpoint_, async_executor_, mock_http_client_,
                           std::move(authorization_http_helper));
  EXPECT_EQ(proxy.Init(), SuccessExecutionResult());
  EXPECT_EQ(proxy.Run(), SuccessExecutionResult());

  EXPECT_CALL(*authorization_http_helper_mock, AddHeadersToRequest(_, _))
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(*mock_http_client_, PerformRequest)
      .WillOnce([](AsyncContext<HttpRequest, HttpResponse>& context) {
        if (context.request == nullptr) ADD_FAILURE();
        context.result = FailureExecutionResult(123);
        context.Finish();
        return SuccessExecutionResult();
      });

  std::atomic<bool> request_finished(false);
  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_request;
  authorization_request.request = make_shared<AuthorizationProxyRequest>();
  authorization_request.request->authorization_metadata =
      authorization_metadata_;
  authorization_request.callback = [&](auto context) {
    request_finished = true;
    EXPECT_EQ(context.result, FailureExecutionResult(123));
    return SuccessExecutionResult();
  };
  EXPECT_EQ(proxy.Authorize(authorization_request), SuccessExecutionResult());
  WaitUntil([&]() { return request_finished.load(); });
}

TEST_F(AuthorizationProxyTest, AuthorizeReturnsRetryIfRequestInProgress) {
  auto authorization_http_helper =
      std::make_unique<HttpRequestResponseAuthInterceptorMock>();

  HttpRequestResponseAuthInterceptorMock* authorization_http_helper_mock =
      authorization_http_helper.get();

  AuthorizationProxy proxy(server_endpoint_, async_executor_, mock_http_client_,
                           std::move(authorization_http_helper));
  EXPECT_EQ(proxy.Init(), SuccessExecutionResult());
  EXPECT_EQ(proxy.Run(), SuccessExecutionResult());

  EXPECT_CALL(*authorization_http_helper_mock, AddHeadersToRequest(_, _))
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(*mock_http_client_, PerformRequest)
      .WillOnce(Return(SuccessExecutionResult()));

  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_request1;
  authorization_request1.request = make_shared<AuthorizationProxyRequest>();
  authorization_request1.request->authorization_metadata =
      authorization_metadata_;

  EXPECT_EQ(proxy.Authorize(authorization_request1), SuccessExecutionResult());

  // Request attempt 2.
  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_request2;
  authorization_request2.request = make_shared<AuthorizationProxyRequest>();
  authorization_request2.request->authorization_metadata =
      authorization_metadata_;

  EXPECT_EQ(proxy.Authorize(authorization_request2),
            RetryExecutionResult(
                errors::SC_AUTHORIZATION_PROXY_AUTH_REQUEST_INPROGRESS));

  // Request attempt 3.
  AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
      authorization_request3;
  authorization_request3.request = make_shared<AuthorizationProxyRequest>();
  authorization_request3.request->authorization_metadata =
      authorization_metadata_;

  EXPECT_EQ(proxy.Authorize(authorization_request3),
            RetryExecutionResult(
                errors::SC_AUTHORIZATION_PROXY_AUTH_REQUEST_INPROGRESS));
}

TEST_F(AuthorizationProxyTest,
       AuthorizeReturnsSuccessAfterRemoteRequestCompletes) {
  auto authorization_http_helper =
      std::make_unique<HttpRequestResponseAuthInterceptorMock>();

  HttpRequestResponseAuthInterceptorMock* authorization_http_helper_mock =
      authorization_http_helper.get();

  AuthorizationProxy proxy(server_endpoint_, async_executor_, mock_http_client_,
                           std::move(authorization_http_helper));
  EXPECT_EQ(proxy.Init(), SuccessExecutionResult());
  EXPECT_EQ(proxy.Run(), SuccessExecutionResult());

  EXPECT_CALL(*authorization_http_helper_mock, AddHeadersToRequest(_, _))
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(*mock_http_client_, PerformRequest)
      .WillOnce([](AsyncContext<HttpRequest, HttpResponse>& context) {
        if (context.request == nullptr) ADD_FAILURE();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*authorization_http_helper_mock,
              ObtainAuthorizedMetadataFromResponse(_))
      .WillOnce([=](const HttpResponse& response) {
        return AuthorizedMetadata{authorized_metadata_.authorized_domain};
      });

  // First request, issues remote HTTP request and cached response.
  {
    std::atomic<bool> request_finished(false);
    AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
        authorization_request;
    authorization_request.request = make_shared<AuthorizationProxyRequest>();
    authorization_request.request->authorization_metadata =
        authorization_metadata_;
    authorization_request.callback = [&](auto context) {
      EXPECT_EQ(context.result, SuccessExecutionResult());
      EXPECT_EQ(*context.response->authorized_metadata.authorized_domain,
                *authorized_metadata_.authorized_domain);
      request_finished = true;
      return SuccessExecutionResult();
    };
    EXPECT_EQ(proxy.Authorize(authorization_request), SuccessExecutionResult());
    WaitUntil([&]() { return request_finished.load(); });
  }

  // Try again, doesn't issue remote HTTP request, but simply returns the cached
  // response.
  {
    std::atomic<bool> request_finished(false);
    AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
        authorization_request;
    authorization_request.request = make_shared<AuthorizationProxyRequest>();
    authorization_request.request->authorization_metadata =
        authorization_metadata_;
    authorization_request.callback = [&](auto context) {
      EXPECT_EQ(context.result, SuccessExecutionResult());
      EXPECT_EQ(*context.response->authorized_metadata.authorized_domain,
                *authorized_metadata_.authorized_domain);
      request_finished = true;
      return SuccessExecutionResult();
    };
    EXPECT_EQ(proxy.Authorize(authorization_request), SuccessExecutionResult());
    EXPECT_EQ(
        *authorization_request.response->authorized_metadata.authorized_domain,
        *authorized_metadata_.authorized_domain);
    WaitUntil([&]() { return request_finished.load(); });
  }
}

TEST_F(AuthorizationProxyTest,
       AuthorizeReturnsFailureDuringParsingRemoteReponseDoesntCacheReponse) {
  auto authorization_http_helper =
      std::make_unique<HttpRequestResponseAuthInterceptorMock>();

  HttpRequestResponseAuthInterceptorMock* authorization_http_helper_mock =
      authorization_http_helper.get();

  AuthorizationProxy proxy(server_endpoint_, async_executor_, mock_http_client_,
                           std::move(authorization_http_helper));
  EXPECT_EQ(proxy.Init(), SuccessExecutionResult());
  EXPECT_EQ(proxy.Run(), SuccessExecutionResult());

  EXPECT_CALL(*authorization_http_helper_mock, AddHeadersToRequest(_, _))
      .Times(2)
      .WillRepeatedly(Return(SuccessExecutionResult()));

  EXPECT_CALL(*mock_http_client_, PerformRequest)
      .Times(2)
      .WillRepeatedly([](AsyncContext<HttpRequest, HttpResponse>& context) {
        if (context.request == nullptr) ADD_FAILURE();
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*authorization_http_helper_mock,
              ObtainAuthorizedMetadataFromResponse(_))
      .WillOnce([=](const HttpResponse& response) {
        return FailureExecutionResult(1234);
      })
      .WillOnce(
          [=](const HttpResponse& response) { return AuthorizedMetadata{}; });

  // Request 1
  {
    std::atomic<bool> request_finished(false);
    AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
        authorization_request;
    authorization_request.request = make_shared<AuthorizationProxyRequest>();
    authorization_request.request->authorization_metadata =
        authorization_metadata_;
    authorization_request.callback = [&](auto context) {
      EXPECT_EQ(context.result, FailureExecutionResult(1234));
      request_finished = true;
      return SuccessExecutionResult();
    };
    EXPECT_EQ(proxy.Authorize(authorization_request), SuccessExecutionResult());
    WaitUntil([&]() { return request_finished.load(); });
  }

  // Request 2
  {
    std::atomic<bool> request_finished(false);
    AsyncContext<AuthorizationProxyRequest, AuthorizationProxyResponse>
        authorization_request;
    authorization_request.request = make_shared<AuthorizationProxyRequest>();
    authorization_request.request->authorization_metadata =
        authorization_metadata_;
    authorization_request.callback = [&](auto context) {
      EXPECT_EQ(context.result, SuccessExecutionResult());
      request_finished = true;
      return SuccessExecutionResult();
    };
    EXPECT_EQ(proxy.Authorize(authorization_request), SuccessExecutionResult());
    WaitUntil([&]() { return request_finished.load(); });
  }
}

}  // namespace google::scp::core::test
