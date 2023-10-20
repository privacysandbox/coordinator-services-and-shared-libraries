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
#include "core/curl_client/src/http1_curl_wrapper.h"

#include <gtest/gtest.h>

#include "core/curl_client/src/error_codes.h"
#include "core/test/utils/http1_helper/test_http1_server.h"
#include "public/core/test/interface/execution_result_matchers.h"

using boost::beast::http::status;
using std::get;
using std::make_shared;
using std::make_tuple;
using std::move;
using std::shared_ptr;
using std::string;
using std::tuple;
using testing::IsSupersetOf;
using testing::Pair;

namespace google::scp::core::test {
namespace {

// Intentionally place a null character before the end and don't place one at
// the end to test null handling. Note that std::string does *not* automatically
// place null characters at the end, except when c_str is called. However, I
// confirmed printing behaves nicely.
constexpr Byte kRequestBody[] = {'a', 'b', '\0', 'c'};
constexpr Byte kResponseBody[] = {'\0', 'd', 'e', 'f'};

class Http1CurlWrapperTest
    : public ::testing::TestWithParam<tuple<status, ExecutionResult>> {
 protected:
  Http1CurlWrapperTest()
      : response_body_(kResponseBody, sizeof(kResponseBody)),
        post_request_body_(kRequestBody, sizeof(kRequestBody)),
        subject_([]() {
          auto wrapper_or = Http1CurlWrapper::MakeWrapper();
          assert(wrapper_or.Successful());
          return move(*wrapper_or);
        }()) {}

  status GetResponseStatusToReturn() { return get<0>(GetParam()); }

  ExecutionResult GetExpectedResult() { return get<1>(GetParam()); }

  const string response_body_;
  const string post_request_body_;

  shared_ptr<Http1CurlWrapper> subject_;

  TestHttp1Server server_;
};

TEST_F(Http1CurlWrapperTest, GetWorks) {
  HttpRequest request;
  request.method = HttpMethod::GET;
  request.path = make_shared<Uri>(server_.GetPath());

  server_.SetResponseBody(BytesBuffer(post_request_body_));

  auto response_or = subject_->PerformRequest(request);
  ASSERT_THAT(response_or, IsSuccessful());
  EXPECT_EQ(response_or->code, errors::HttpStatusCode::OK);
  EXPECT_EQ(response_or->body.ToString(), post_request_body_);

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
}

TEST_F(Http1CurlWrapperTest, GetWorksWithHeaders) {
  HttpRequest request;
  request.method = HttpMethod::GET;
  request.path = make_shared<Uri>(server_.GetPath());
  request.headers = make_shared<HttpHeaders>();
  request.headers->insert({"key1", "val1"});
  request.headers->insert({"key2", "val2"});

  server_.SetResponseBody(BytesBuffer(response_body_));
  server_.SetResponseHeaders(
      HttpHeaders({{"resp1", "resp_val1"}, {"resp2", "resp_val2"}}));

  auto response_or = subject_->PerformRequest(request);
  ASSERT_THAT(response_or, IsSuccessful());
  EXPECT_EQ(response_or->code, errors::HttpStatusCode::OK);
  EXPECT_EQ(response_or->body.ToString(), response_body_);

  EXPECT_THAT(
      *response_or->headers,
      IsSupersetOf({Pair("resp1", "resp_val1"), Pair("resp2", "resp_val2")}));

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);

  EXPECT_THAT(GetRequestHeadersMap(server_.Request()),
              IsSupersetOf({Pair("key1", "val1"), Pair("key2", "val2")}));
}

TEST_F(Http1CurlWrapperTest, PostWorks) {
  HttpRequest request;
  request.method = HttpMethod::POST;
  request.path = make_shared<Uri>(server_.GetPath());
  request.body = BytesBuffer(post_request_body_);

  server_.SetResponseBody(BytesBuffer(response_body_));

  auto response_or = subject_->PerformRequest(request);
  ASSERT_THAT(response_or, IsSuccessful());
  EXPECT_EQ(response_or->code, errors::HttpStatusCode::OK);
  EXPECT_EQ(response_or->body.ToString(), response_body_);

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::post);
  EXPECT_EQ(server_.RequestBody(), post_request_body_);
}

TEST_F(Http1CurlWrapperTest, PutWorks) {
  HttpRequest request;
  request.method = HttpMethod::PUT;
  request.path = make_shared<Uri>(server_.GetPath());
  request.body = BytesBuffer(post_request_body_);

  server_.SetResponseBody(BytesBuffer(response_body_));

  auto response_or = subject_->PerformRequest(request);
  ASSERT_THAT(response_or, IsSuccessful());
  EXPECT_EQ(response_or->code, errors::HttpStatusCode::OK);
  EXPECT_EQ(response_or->body.ToString(), response_body_);

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::put);
  EXPECT_EQ(server_.RequestBody(), post_request_body_);
}

TEST_F(Http1CurlWrapperTest, PostWorksWithHeaders) {
  HttpRequest request;
  request.method = HttpMethod::POST;
  request.path = make_shared<Uri>(server_.GetPath());
  request.body = BytesBuffer(post_request_body_);
  request.headers = make_shared<HttpHeaders>();
  request.headers->insert({"key1", "val1"});
  request.headers->insert({"key2", "val2"});

  server_.SetResponseBody(BytesBuffer(response_body_));
  server_.SetResponseHeaders(
      HttpHeaders({{"resp1", "resp_val1"}, {"resp2", "resp_val2"}}));

  auto response_or = subject_->PerformRequest(request);
  ASSERT_THAT(response_or, IsSuccessful());
  EXPECT_EQ(response_or->code, errors::HttpStatusCode::OK);
  EXPECT_EQ(response_or->body.ToString(), response_body_);

  EXPECT_THAT(
      *response_or->headers,
      IsSupersetOf({Pair("resp1", "resp_val1"), Pair("resp2", "resp_val2")}));

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::post);
  EXPECT_EQ(server_.RequestBody(), post_request_body_);

  EXPECT_THAT(GetRequestHeadersMap(server_.Request()),
              IsSupersetOf({Pair("key1", "val1"), Pair("key2", "val2")}));
}

TEST_F(Http1CurlWrapperTest, SingleQueryIsEscaped) {
  HttpRequest request;
  request.method = HttpMethod::GET;
  request.path = make_shared<Uri>(server_.GetPath());
  request.query = make_shared<string>("foo=!@#$");

  auto response_or = subject_->PerformRequest(request);
  ASSERT_THAT(response_or, IsSuccessful());
  EXPECT_EQ(response_or->code, errors::HttpStatusCode::OK);

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
  // '=' should not be escaped.
  EXPECT_EQ(server_.Request().target(), "/?foo=%21%40%23%24");
}

TEST_F(Http1CurlWrapperTest, MultiQueryIsEscaped) {
  HttpRequest request;
  request.method = HttpMethod::GET;
  request.path = make_shared<Uri>(server_.GetPath());
  request.query = make_shared<string>("foo=!@#$&bar=%^()");

  auto response_or = subject_->PerformRequest(request);
  ASSERT_THAT(response_or, IsSuccessful());
  EXPECT_EQ(response_or->code, errors::HttpStatusCode::OK);

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
  // '=' should not be escaped.
  EXPECT_EQ(server_.Request().target(), "/?foo=%21%40%23%24&bar=%25%5E%28%29");
}

TEST_F(Http1CurlWrapperTest, GetPostGetWorks) {
  // Get 1.
  {
    HttpRequest request;
    request.method = HttpMethod::GET;
    request.path = make_shared<Uri>(server_.GetPath());

    server_.SetResponseBody(BytesBuffer(response_body_));

    auto response_or = subject_->PerformRequest(request);
    ASSERT_THAT(response_or, IsSuccessful());
    EXPECT_EQ(response_or->code, errors::HttpStatusCode::OK);
    EXPECT_EQ(response_or->body.ToString(), response_body_);

    EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
    EXPECT_EQ(server_.RequestBody(), "");
  }
  // Post.
  {
    HttpRequest request;
    request.method = HttpMethod::POST;
    request.path = make_shared<Uri>(server_.GetPath());
    request.body = BytesBuffer(post_request_body_);

    server_.SetResponseBody(BytesBuffer(response_body_));

    auto response_or = subject_->PerformRequest(request);
    ASSERT_THAT(response_or, IsSuccessful());
    EXPECT_EQ(response_or->code, errors::HttpStatusCode::OK);
    EXPECT_EQ(response_or->body.ToString(), response_body_);

    EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::post);
    EXPECT_EQ(server_.RequestBody(), post_request_body_);
  }
  // Get 2.
  {
    HttpRequest request;
    request.method = HttpMethod::GET;
    request.path = make_shared<Uri>(server_.GetPath());

    server_.SetResponseBody(BytesBuffer(response_body_));

    auto response_or = subject_->PerformRequest(request);
    ASSERT_THAT(response_or, IsSuccessful());
    EXPECT_EQ(response_or->code, errors::HttpStatusCode::OK);
    EXPECT_EQ(response_or->body.ToString(), response_body_);

    EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
    EXPECT_EQ(server_.RequestBody(), "");
  }
}

TEST_P(Http1CurlWrapperTest, PropagatesHttpError) {
  HttpRequest request;
  request.method = HttpMethod::GET;
  request.path = make_shared<Uri>(server_.GetPath());

  server_.SetResponseStatus(GetResponseStatusToReturn());

  auto response_or = subject_->PerformRequest(request);
  ASSERT_THAT(response_or, ResultIs(GetExpectedResult()));

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
}

INSTANTIATE_TEST_SUITE_P(
    HttpErrors, Http1CurlWrapperTest,
    testing::Values(
        make_tuple(status::unauthorized,
                   FailureExecutionResult(
                       errors::SC_CURL_CLIENT_REQUEST_UNAUTHORIZED)),
        make_tuple(
            status::forbidden,
            FailureExecutionResult(errors::SC_CURL_CLIENT_REQUEST_FORBIDDEN)),
        make_tuple(
            status::not_found,
            FailureExecutionResult(errors::SC_CURL_CLIENT_REQUEST_NOT_FOUND)),
        make_tuple(
            status::conflict,
            FailureExecutionResult(errors::SC_CURL_CLIENT_REQUEST_CONFLICT)),
        make_tuple(
            status::internal_server_error,
            RetryExecutionResult(errors::SC_CURL_CLIENT_REQUEST_SERVER_ERROR)),
        make_tuple(status::not_implemented,
                   RetryExecutionResult(
                       errors::SC_CURL_CLIENT_REQUEST_NOT_IMPLEMENTED)),
        make_tuple(status::service_unavailable,
                   RetryExecutionResult(
                       errors::SC_CURL_CLIENT_REQUEST_SERVICE_UNAVAILABLE)),
        // This one is not enumerated.
        make_tuple(status::payment_required,
                   RetryExecutionResult(
                       errors::SC_CURL_CLIENT_REQUEST_OTHER_HTTP_ERROR)),
        make_tuple(status::ok, SuccessExecutionResult())));

}  // namespace
}  // namespace google::scp::core::test
