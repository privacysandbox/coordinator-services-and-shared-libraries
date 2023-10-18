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

#include "roma/sandbox/js_engine/src/v8_engine/snapshot_compilation_context.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::FailureExecutionResult;
using google::scp::core::test::ResultIs;
using google::scp::roma::sandbox::js_engine::v8_js_engine::
    SnapshotCompilationContext;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;
using testing::IsNull;
using testing::NotNull;
using v8::Isolate;

namespace google::scp::roma::sandbox::js_engine::test {
class SnapshotCompilationContextTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const int my_pid = getpid();
    const string proc_exe_path = string("/proc/") + to_string(my_pid) + "/exe";
    auto my_path = std::make_unique<char[]>(PATH_MAX);
    ssize_t sz = readlink(proc_exe_path.c_str(), my_path.get(), PATH_MAX);
    ASSERT_GT(sz, 0);
    v8::V8::InitializeICUDefaultLocation(my_path.get());
    v8::V8::InitializeExternalStartupData(my_path.get());
    platform_ = v8::platform::NewDefaultPlatform().release();
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
  }

  shared_ptr<SnapshotCompilationContext> CreateCompilationContext() {
    auto context = make_shared<SnapshotCompilationContext>();
    context->v8_isolate = CreateIsolate();
    return context;
  }

  std::vector<v8::Isolate*> created_isolates;
  std::vector<shared_ptr<void>> create_contexts;

  Isolate* CreateIsolate() {
    Isolate::CreateParams params;
    params.array_buffer_allocator =
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    auto isolate = Isolate::New(params);

    created_isolates.emplace_back(isolate);
    return isolate;
  }

  static v8::Platform* platform_;
};

v8::Platform* SnapshotCompilationContextTest::platform_{nullptr};

TEST_F(SnapshotCompilationContextTest, IsolateShouldDisposeAfterNoRefers) {
  create_contexts.reserve(5);

  for (auto i = 0; i < 5; i++) {
    create_contexts.emplace_back(CreateCompilationContext());
  }
  for (auto i = 0; i < 5; i++) {
    // The isolates are initialized.
    EXPECT_THAT(created_isolates[i]->GetHeapProfiler(), NotNull());
  }

  EXPECT_EQ(create_contexts.size(), 5);
  EXPECT_EQ(created_isolates.size(), 5);

  // Clear the contexts from vector which will remove all refers of the context.
  create_contexts.clear();

  EXPECT_EQ(create_contexts.size(), 0);
  EXPECT_EQ(created_isolates.size(), 5);

  for (auto i = 0; i < 5; i++) {
    // The isolates are disposed.
    EXPECT_THAT(created_isolates[i]->GetHeapProfiler(), IsNull());
  }
}

}  // namespace google::scp::roma::sandbox::js_engine::test
