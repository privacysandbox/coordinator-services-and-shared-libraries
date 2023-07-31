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

#include "roma/ipc/src/ipc_message.h"

#include <gtest/gtest.h>

#include <atomic>

#include "core/test/utils/conditional_wait.h"
#include "roma/common/src/shared_memory.h"
#include "roma/common/src/shared_memory_pool.h"

using google::scp::core::test::WaitUntil;
using std::atomic;
using std::make_shared;
using std::string;
using std::vector;

namespace google::scp::roma::ipc::test {
using common::SharedMemoryPool;
using common::SharedMemorySegment;

class IpcMessageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    segment_.Create(10240);
    pool_.Init(segment_.Get(), segment_.Size());
    pool_.SetThisThreadMemPool();
  }

  void TearDown() override { segment_.Unmap(); }

  SharedMemorySegment segment_;
  SharedMemoryPool pool_;
};

TEST_F(IpcMessageTest, Share) {
  auto* vec = new (pool_.Allocate(sizeof(RomaVector<RomaCodeObj>)))
      RomaVector<RomaCodeObj>();
  auto* ready = new (pool_.Allocate(sizeof(atomic<bool>))) atomic<bool>(false);
  pid_t pid = fork();
  if (pid == 0) {
    CodeObject code_obj;
    code_obj.id = "id";
    code_obj.version_num = 1;
    code_obj.js = "hello world";
    code_obj.wasm = "hello world";
    code_obj.tags.insert({"key", "value"});
    vec->emplace_back(code_obj);

    InvocationRequestSharedInput execution_obj;
    execution_obj.id = "id";
    execution_obj.version_num = 1;
    execution_obj.input.push_back(make_shared<string>("arg0"));
    execution_obj.tags.insert({"key", "value"});
    vec->emplace_back(execution_obj);
    ready->store(true);
    exit(0);
  }
  WaitUntil([=]() { return ready->load(); });

  auto& roma_obj_1 = vec->at(0);
  EXPECT_EQ(string(roma_obj_1.id), "id");
  EXPECT_EQ(roma_obj_1.version_num, 1);
  EXPECT_EQ(string(roma_obj_1.js), "hello world");
  EXPECT_EQ(string(roma_obj_1.wasm), "hello world");
  EXPECT_EQ(roma_obj_1.tags.size(), 1);
  EXPECT_EQ(string(roma_obj_1.tags["key"]), "value");

  auto& roma_obj_2 = vec->at(1);
  EXPECT_EQ(string(roma_obj_2.id), "id");
  EXPECT_EQ(roma_obj_2.version_num, 1);
  EXPECT_EQ(roma_obj_2.input.size(), 1);
  EXPECT_EQ(string(roma_obj_2.input[0]), "arg0");
  EXPECT_EQ(roma_obj_2.tags.size(), 1);
  EXPECT_EQ(string(roma_obj_2.tags["key"]), "value");
}
}  // namespace google::scp::roma::ipc::test
