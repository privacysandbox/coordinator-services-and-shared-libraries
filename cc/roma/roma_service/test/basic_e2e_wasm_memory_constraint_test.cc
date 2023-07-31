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

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>

#include "core/test/utils/conditional_wait.h"
#include "roma/interface/roma.h"
#include "roma/wasm/test/testing_utils.h"

using absl::StatusOr;
using google::scp::core::test::WaitUntil;
using google::scp::roma::wasm::testing::WasmTestingUtils;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::string;
using std::to_string;
using std::unique_ptr;

/**
 * @brief This test needs to run separately since if modifies the roma
 * environment and even stopping and starting roma afterwards won't fix it if
 * it's running with other processes, since some v8 configurations live until
 * the process exits.
 *
 */
namespace google::scp::roma::test {
/**
 * @brief Based on empirical testing, we can always allocate an amount close to
 * half of the total module memory.
 */
TEST(RomaBasicE2ETest,
     WasmAllocationShouldFailEvenIfModuleHasLargeMemoryWhenConfiguredToLower) {
  Config config;
  // 80 pages equals 5MiB since each page is 64KiB.
  config.MaxWasmMemoryNumberOfPages = 80;

  auto status = RomaInit(config);
  EXPECT_TRUE(status.ok());

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/cpp_wasm_allocate_memory/allocate_memory.wasm");

  atomic<bool> load_finished = false;
  atomic<bool> execute_finished = false;
  {
    auto code_obj = make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_num = 1;
    code_obj->js = "";
    code_obj->wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                          wasm_bin.size());

    status = LoadCodeObj(move(code_obj),
                         [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                           EXPECT_TRUE(resp->ok());
                           load_finished.store(true);
                         });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = make_unique<InvocationRequestSharedInput>();
    execution_obj->id = "foo";
    execution_obj->version_num = 1;
    execution_obj->handler_name = "Handler";
    execution_obj->wasm_return_type = WasmDataType::kUint32;

    // This module was compiled with an overall 10MB memory size, so a 4.99 MB
    // allocation would generally succeed, however, we limit the memory in the
    // config. So the module initialization fails, hence the request fails.
    constexpr uint32_t size_to_allocate = (4.99 * 1024 * 1024);  // 4.99MB
    execution_obj->input.push_back(
        make_shared<string>(to_string(size_to_allocate)));

    status = Execute(
        move(execution_obj),
        [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          // Fails
          EXPECT_FALSE(resp->ok());
          EXPECT_EQ("Failed to create wasm object.", resp->status().message());
          execute_finished.store(true);
        });
    EXPECT_TRUE(status.ok());
  }
  WaitUntil([&]() { return load_finished.load(); });
  WaitUntil([&]() { return execute_finished.load(); });

  status = RomaStop();
  EXPECT_TRUE(status.ok());
}
}  // namespace google::scp::roma::test
