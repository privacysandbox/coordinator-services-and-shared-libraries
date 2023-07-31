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

#include "roma/ipc/src/ipc_channel.h"

#include <gtest/gtest.h>

#include <memory>

#include "core/test/utils/auto_init_run_stop.h"
#include "roma/common/src/shared_memory.h"
#include "roma/common/src/shared_memory_pool.h"

using google::scp::core::test::AutoInitRunStop;
using std::make_unique;
using std::move;
using std::unique_ptr;

namespace google::scp::roma::ipc::test {
using common::SharedMemoryPool;
using common::SharedMemorySegment;

class IpcChannelTest : public ::testing::Test {
 protected:
  void SetUp() override { segment_.Create(100240); }

  void TearDown() override { segment_.Unmap(); }

  SharedMemorySegment segment_;
};

TEST_F(IpcChannelTest, ShouldReturnFailureWhenLastCodeObjectIsEmpty) {
  IpcChannel channel(segment_);
  AutoInitRunStop auto_init_run_stop(channel);
  channel.GetMemPool().SetThisThreadMemPool();

  unique_ptr<RomaCodeObj> last_code_obj;
  EXPECT_FALSE(channel.GetLastRecordedCodeObjectWithoutInputs(last_code_obj)
                   .Successful());
}

TEST_F(IpcChannelTest, ShouldReturnLastCodeObjectAfterItsRecorded) {
  IpcChannel channel(segment_);
  AutoInitRunStop auto_init_run_stop(channel);
  channel.GetMemPool().SetThisThreadMemPool();

  auto code_obj = make_unique<CodeObject>();
  code_obj->id = "MyId123";
  code_obj->version_num = 1;
  code_obj->js = "JS";
  Callback callback;
  auto request_to_push = make_unique<Request>(move(code_obj), callback);
  EXPECT_TRUE(channel.TryAcquirePushRequest().Successful());
  EXPECT_TRUE(channel.PushRequest(move(request_to_push)).Successful());

  // Should still be empty
  unique_ptr<RomaCodeObj> last_code_obj;
  EXPECT_FALSE(channel.GetLastRecordedCodeObjectWithoutInputs(last_code_obj)
                   .Successful());

  Request* request;
  // The pop makes it be recorded
  EXPECT_TRUE(channel.PopRequest(request).Successful());

  EXPECT_TRUE(channel.GetLastRecordedCodeObjectWithoutInputs(last_code_obj)
                  .Successful());
  EXPECT_STREQ(last_code_obj->id.c_str(), "MyId123");
}

TEST_F(IpcChannelTest, ShouldNotUpdateLastCodeObjectIfEmpty) {
  IpcChannel channel(segment_);
  AutoInitRunStop auto_init_run_stop(channel);
  channel.GetMemPool().SetThisThreadMemPool();

  // Empty code object (no JS or WASM)
  auto code_obj = make_unique<CodeObject>();
  code_obj->id = "MyId123";
  code_obj->version_num = 1;
  Callback callback;
  auto request_to_push = make_unique<Request>(move(code_obj), callback);
  EXPECT_TRUE(channel.TryAcquirePushRequest().Successful());
  EXPECT_TRUE(channel.PushRequest(move(request_to_push)).Successful());

  // Should be empty
  unique_ptr<RomaCodeObj> last_code_obj;
  EXPECT_FALSE(channel.GetLastRecordedCodeObjectWithoutInputs(last_code_obj)
                   .Successful());

  Request* request;
  EXPECT_TRUE(channel.PopRequest(request).Successful());

  // Should still be empty
  EXPECT_FALSE(channel.GetLastRecordedCodeObjectWithoutInputs(last_code_obj)
                   .Successful());
}

TEST_F(IpcChannelTest, ShouldUpdateLastCodeObjectIfVersionChanges) {
  IpcChannel channel(segment_);
  AutoInitRunStop auto_init_run_stop(channel);
  channel.GetMemPool().SetThisThreadMemPool();

  auto code_obj = make_unique<CodeObject>();
  code_obj->id = "MyId123";
  code_obj->version_num = 1;
  code_obj->js = "JS";
  Callback callback;
  auto request_to_push = make_unique<Request>(move(code_obj), callback);
  EXPECT_TRUE(channel.TryAcquirePushRequest().Successful());
  EXPECT_TRUE(channel.PushRequest(move(request_to_push)).Successful());

  Request* request;
  EXPECT_TRUE(channel.PopRequest(request).Successful());
  auto resp = make_unique<Response>();
  // Respond to request to be able to pop next available request
  EXPECT_TRUE(channel.PushResponse(move(resp)));

  unique_ptr<RomaCodeObj> last_code_obj;

  EXPECT_TRUE(channel.GetLastRecordedCodeObjectWithoutInputs(last_code_obj)
                  .Successful());
  EXPECT_STREQ(last_code_obj->id.c_str(), "MyId123");
  EXPECT_EQ(last_code_obj->version_num, 1);
  EXPECT_STREQ(last_code_obj->js.c_str(), "JS");

  code_obj = make_unique<CodeObject>();
  code_obj->id = "MyId123";
  code_obj->version_num = 2;
  code_obj->js = "NewJS";
  request_to_push = make_unique<Request>(move(code_obj), callback);
  EXPECT_TRUE(channel.TryAcquirePushRequest().Successful());
  EXPECT_TRUE(channel.PushRequest(move(request_to_push)).Successful());

  EXPECT_TRUE(channel.PopRequest(request).Successful());

  EXPECT_TRUE(channel.GetLastRecordedCodeObjectWithoutInputs(last_code_obj)
                  .Successful());
  EXPECT_STREQ(last_code_obj->id.c_str(), "MyId123");
  EXPECT_EQ(last_code_obj->version_num, 2);
  EXPECT_STREQ(last_code_obj->js.c_str(), "NewJS");
}

TEST_F(IpcChannelTest, ShouldNotUpdateLastCodeObjectIfVersionDoesNotChange) {
  IpcChannel channel(segment_);
  AutoInitRunStop auto_init_run_stop(channel);
  channel.GetMemPool().SetThisThreadMemPool();

  auto code_obj = make_unique<CodeObject>();
  code_obj->id = "MyId123";
  code_obj->version_num = 1;
  code_obj->js = "OldJS";
  Callback callback;
  auto request_to_push = make_unique<Request>(move(code_obj), callback);
  EXPECT_TRUE(channel.TryAcquirePushRequest().Successful());
  EXPECT_TRUE(channel.PushRequest(move(request_to_push)).Successful());

  Request* request;
  EXPECT_TRUE(channel.PopRequest(request).Successful());
  auto resp = make_unique<Response>();
  // Respond to request to be able to pop next available request
  EXPECT_TRUE(channel.PushResponse(move(resp)));

  unique_ptr<RomaCodeObj> last_code_obj;

  EXPECT_TRUE(channel.GetLastRecordedCodeObjectWithoutInputs(last_code_obj)
                  .Successful());
  EXPECT_STREQ(last_code_obj->id.c_str(), "MyId123");
  EXPECT_EQ(last_code_obj->version_num, 1);
  EXPECT_STREQ(last_code_obj->js.c_str(), "OldJS");

  code_obj = make_unique<CodeObject>();
  code_obj->id = "MyId123";
  // Same version
  code_obj->version_num = 1;
  code_obj->js = "NewJS";
  request_to_push = make_unique<Request>(move(code_obj), callback);
  EXPECT_TRUE(channel.TryAcquirePushRequest().Successful());
  EXPECT_TRUE(channel.PushRequest(move(request_to_push)).Successful());

  EXPECT_TRUE(channel.PopRequest(request).Successful());

  EXPECT_TRUE(channel.GetLastRecordedCodeObjectWithoutInputs(last_code_obj)
                  .Successful());
  EXPECT_STREQ(last_code_obj->id.c_str(), "MyId123");
  EXPECT_EQ(last_code_obj->version_num, 1);
  EXPECT_STREQ(last_code_obj->js.c_str(), "OldJS");
}
}  // namespace google::scp::roma::ipc::test
