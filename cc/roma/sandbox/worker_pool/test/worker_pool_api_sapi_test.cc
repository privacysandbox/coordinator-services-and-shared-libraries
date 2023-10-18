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

#include "roma/sandbox/worker_pool/src/worker_pool_api_sapi.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "public/core/test/interface/execution_result_matchers.h"
#include "roma/sandbox/worker_api/src/worker_api_sapi.h"

using google::scp::roma::sandbox::worker_api::WorkerApiSapi;
using google::scp::roma::sandbox::worker_api::WorkerApiSapiConfig;
using std::string;
using std::vector;

namespace google::scp::roma::sandbox::worker_pool::test {
TEST(WorkerPoolTest, CanInitRunAndStop) {
  int num_workers = 4;
  vector<WorkerApiSapiConfig> configs;
  for (int i = 0; i < num_workers; i++) {
    WorkerApiSapiConfig config;
    config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
    config.js_engine_require_code_preload = false;
    config.native_js_function_comms_fd = -1;
    config.native_js_function_names = vector<string>();
    configs.push_back(config);
  }

  auto pool = WorkerPoolApiSapi(configs, num_workers);

  auto result = pool.Init();
  EXPECT_SUCCESS(result);

  result = pool.Run();
  EXPECT_SUCCESS(result);

  result = pool.Stop();
  EXPECT_SUCCESS(result);
}

TEST(WorkerPoolTest, CanGetPoolCount) {
  int num_workers = 2;
  vector<WorkerApiSapiConfig> configs;
  for (int i = 0; i < num_workers; i++) {
    WorkerApiSapiConfig config;
    config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
    config.js_engine_require_code_preload = false;
    config.native_js_function_comms_fd = -1;
    config.native_js_function_names = vector<string>();
    configs.push_back(config);
  }

  auto pool = WorkerPoolApiSapi(configs, num_workers);

  auto result = pool.Init();
  EXPECT_SUCCESS(result);

  result = pool.Run();
  EXPECT_SUCCESS(result);

  EXPECT_EQ(pool.GetPoolSize(), 2);

  result = pool.Stop();
  EXPECT_SUCCESS(result);
}

TEST(WorkerPoolTest, CanGetWorker) {
  int num_workers = 2;
  vector<WorkerApiSapiConfig> configs;
  for (int i = 0; i < num_workers; i++) {
    WorkerApiSapiConfig config;
    config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
    config.js_engine_require_code_preload = false;
    config.native_js_function_comms_fd = -1;
    config.native_js_function_names = vector<string>();
    configs.push_back(config);
  }

  auto pool = WorkerPoolApiSapi(configs, num_workers);

  auto result = pool.Init();
  EXPECT_SUCCESS(result);

  result = pool.Run();
  EXPECT_SUCCESS(result);

  auto worker1 = pool.GetWorker(0);
  EXPECT_SUCCESS(worker1.result());
  auto worker2 = pool.GetWorker(1);
  EXPECT_SUCCESS(worker2.result());

  EXPECT_NE(worker1->get(), worker2->get());

  result = pool.Stop();
  EXPECT_SUCCESS(result);
}

TEST(WorkerPoolTest, ConstructorFailsIfSizeIsInvalid) {
  constexpr size_t size = 2;

  // Pool of size 2, but no configs
  EXPECT_DEATH(WorkerPoolApiSapi(vector<WorkerApiSapiConfig>(), size),
               "ROMA: The worker config vector and the pool size do not match");
}
}  // namespace google::scp::roma::sandbox::worker_pool::test
