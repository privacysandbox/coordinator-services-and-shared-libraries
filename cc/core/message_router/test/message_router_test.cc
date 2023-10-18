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

#include "core/message_router/src/message_router.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "core/common/concurrent_queue/src/concurrent_queue.h"
#include "core/interface/async_context.h"
#include "core/message_router/src/error_codes.h"
#include "core/message_router/test/test.pb.h"
#include "core/test/utils/conditional_wait.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using core::message_router::test::TestBoolRequest;
using core::message_router::test::TestBoolResponse;
using core::message_router::test::TestStringRequest;
using core::message_router::test::TestStringResponse;
using google::protobuf::Any;
using google::scp::core::common::ConcurrentQueue;
using std::atomic;
using std::make_shared;

namespace google::scp::core::test {
class MessageRouterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_SUCCESS(router_.Init());
    EXPECT_SUCCESS(router_.Run());

    running_ = true;
    thread_ = std::thread([this]() {
      while (running_) {
        std::shared_ptr<AsyncContext<Any, Any>> context;
        auto dequeue_result = queue_->TryDequeue(context);
        if (dequeue_result.Successful()) {
          router_.OnMessageReceived(context);
        } else {
          std::this_thread::yield();
        }
      }
    });

    TestStringRequest test_request_1;
    any_request_1_.PackFrom(test_request_1);
    TestBoolRequest test_request_2;
    any_request_2_.PackFrom(test_request_2);
  }

  void TearDown() override {
    running_ = false;
    thread_.join();
    EXPECT_SUCCESS(router_.Stop());
  }

  std::shared_ptr<ConcurrentQueue<std::shared_ptr<AsyncContext<Any, Any>>>>
      queue_ =
          make_shared<ConcurrentQueue<std::shared_ptr<AsyncContext<Any, Any>>>>(
              10);
  MessageRouter router_;
  Any any_request_1_;
  Any any_request_2_;
  std::thread thread_;
  bool running_;
};

TEST_F(MessageRouterTest, RequestNotSubscribed) {
  atomic<int> count(0);
  auto request = make_shared<Any>(any_request_1_);
  auto context = make_shared<AsyncContext<Any, Any>>(
      request, [&](AsyncContext<Any, Any>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_MESSAGE_ROUTER_REQUEST_NOT_SUBSCRIBED)));
        count++;
      });
  queue_->TryEnqueue(context);

  WaitUntil([&]() { return count == 1; });
  EXPECT_EQ(count, 1);
}

TEST_F(MessageRouterTest, SubscriptionConflict) {
  auto request_type = any_request_1_.type_url();
  EXPECT_SUCCESS(
      router_.Subscribe(request_type, [&](AsyncContext<Any, Any>& context) {}));

  EXPECT_THAT(
      router_.Subscribe(request_type, [&](AsyncContext<Any, Any>& context) {}),
      ResultIs(FailureExecutionResult(
          errors::SC_MESSAGE_ROUTER_REQUEST_ALREADY_SUBSCRIBED)));
}

TEST_F(MessageRouterTest, SingleMessage) {
  auto request = make_shared<Any>(any_request_1_);
  router_.Subscribe(any_request_1_.type_url(),
                    [&](AsyncContext<Any, Any>& context) {
                      TestStringResponse response;
                      response.set_response("test_response");
                      Any any_response;
                      any_response.PackFrom(response);
                      context.response = make_shared<Any>(any_response);
                    });
  auto context = make_shared<AsyncContext<Any, Any>>(
      request, [&](AsyncContext<Any, Any>& context) {});
  queue_->TryEnqueue(context);

  WaitUntil([&]() { return context->response != nullptr; });
  TestStringResponse response;
  context->response->UnpackTo(&response);
  EXPECT_EQ(response.response(), "test_response");
}

TEST_F(MessageRouterTest, MultipleMessagesSingleSubscription) {
  router_.Subscribe(any_request_1_.type_url(),
                    [&](AsyncContext<Any, Any>& context) {
                      TestStringResponse response;
                      response.set_response("test_response");
                      Any any_response;
                      any_response.PackFrom(response);
                      context.response = make_shared<Any>(any_response);
                    });
  auto request_1 = make_shared<Any>(any_request_1_);
  auto context_1 = make_shared<AsyncContext<Any, Any>>(
      request_1, [&](AsyncContext<Any, Any>& context) {});
  auto request_2 = make_shared<Any>(any_request_1_);
  auto context_2 = make_shared<AsyncContext<Any, Any>>(
      request_2, [&](AsyncContext<Any, Any>& context) {});
  queue_->TryEnqueue(context_1);
  queue_->TryEnqueue(context_2);

  WaitUntil([&]() {
    return context_1->response != nullptr && context_2->response != nullptr;
  });
  TestStringResponse response_1;
  context_1->response->UnpackTo(&response_1);
  EXPECT_EQ(response_1.response(), "test_response");
  TestStringResponse response_2;
  context_2->response->UnpackTo(&response_2);
  EXPECT_EQ(response_2.response(), "test_response");
}

TEST_F(MessageRouterTest, MultipleSubscriptions) {
  atomic<int> count_1(0);
  router_.Subscribe(any_request_1_.type_url(),
                    [&](AsyncContext<Any, Any>& context) { count_1++; });
  auto request_1 = make_shared<Any>(any_request_1_);
  auto context_1 = make_shared<AsyncContext<Any, Any>>(
      request_1, [&](AsyncContext<Any, Any>& context) {});

  atomic<int> count_2(0);
  router_.Subscribe(any_request_2_.type_url(),
                    [&](AsyncContext<Any, Any>& context) { count_2++; });
  auto request_2 = make_shared<Any>(any_request_2_);
  auto context_2 = make_shared<AsyncContext<Any, Any>>(
      request_2, [&](AsyncContext<Any, Any>& context) {});

  queue_->TryEnqueue(context_1);
  queue_->TryEnqueue(context_2);

  WaitUntil([&]() { return count_1 == 1 && count_2 == 1; });
  EXPECT_EQ(count_1, 1);
  EXPECT_EQ(count_2, 1);
}
}  // namespace google::scp::core::test
