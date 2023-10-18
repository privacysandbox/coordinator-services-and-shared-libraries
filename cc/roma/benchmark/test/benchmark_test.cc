/*
 * Copyright 2023 Google LLC
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "core/common/time_provider/src/stopwatch.h"
#include "core/test/utils/conditional_wait.h"
#include "roma/config/src/config.h"
#include "roma/interface/roma.h"

using absl::StatusOr;
using google::scp::core::common::Stopwatch;
using google::scp::core::test::WaitUntil;
using std::accumulate;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::sort;
using std::string;
using std::thread;
using std::unique_ptr;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::seconds;

using namespace std::chrono_literals;  // NOLINT

namespace google::scp::roma::test {

static void LoadCode(size_t code_bloat_size = 1000) {
  auto code_obj = make_unique<CodeObject>();
  code_obj->id = "foo";
  code_obj->version_num = 1;
  code_obj->js = R"JS_CODE(
    function Handler(input) {
      return "Hello, World!";
    };
    )JS_CODE";

  string bloat(code_bloat_size, 'A');
  code_obj->js += "bloat = \"" + bloat + "\";";

  atomic<bool> load_finished = false;

  auto status = LoadCodeObj(move(code_obj),
                            [&](unique_ptr<StatusOr<ResponseObject>> resp) {
                              EXPECT_TRUE(resp->ok());
                              load_finished.store(true);
                            });
  EXPECT_TRUE(status.ok());

  WaitUntil([&]() { return load_finished.load(); });
}

static void ExecuteCode(const shared_ptr<string>& input) {
  auto code_obj = make_unique<InvocationRequestSharedInput>();
  code_obj->id = "foo";
  code_obj->version_num = 1;
  code_obj->handler_name = "Handler";
  code_obj->input.push_back(input);

  atomic<bool> execute_finished = false;
  string result = "";

  auto status =
      Execute(move(code_obj), [&](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        if (resp->ok()) {
          auto& code_resp = **resp;
          result = code_resp.resp;
        }
        execute_finished.store(true);
      });

  WaitUntil([&]() { return execute_finished.load(); }, 1000s);

  EXPECT_TRUE(status.ok());
  EXPECT_EQ("\"Hello, World!\"", result);
}

static void RunLoad(size_t number_of_requests, size_t number_of_threads,
                    size_t input_size, vector<vector<uint64_t>>* exec_times) {
  auto requests_per_thread = number_of_requests / number_of_threads;

  string input_bloat(input_size, 'A');
  auto input = make_shared<string>("\"" + input_bloat + "\"");

  for (int i = 0; i < number_of_threads; i++) {
    exec_times->at(i).reserve(requests_per_thread);
  }

  auto threads = vector<thread>();
  threads.reserve(number_of_threads);

  for (int i = 0; i < number_of_threads; i++) {
    threads.push_back(thread([i, input, requests_per_thread, &exec_times]() {
      auto& time = exec_times->at(i);
      auto requests_left = requests_per_thread;
      Stopwatch stopwatch;

      while (requests_left-- > 0) {
        stopwatch.Start();

        ExecuteCode(input);

        time.push_back(stopwatch.Stop().count());
      }
    }));
  }

  for (auto& t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }
}

static void DumpStats(vector<int> percentiles,
                      const vector<vector<uint64_t>>& data) {
  vector<uint64_t> combined;
  size_t total_size = 0;
  for (auto& v : data) {
    total_size += v.size();
  }
  combined.reserve(total_size);

  for (auto& v : data) {
    combined.insert(combined.end(), v.begin(), v.end());
  }

  auto avg = accumulate(combined.begin(), combined.end(), (uint64_t)0) /
             combined.size();

  std::cout << "Average: " << avg << " ns" << std::endl;

  sort(combined.begin(), combined.end());

  for (auto& p : percentiles) {
    auto index = combined.size() / 100 * p;

    std::cout << p << "th percentile: " << combined.at(index) << " ns"
              << std::endl;
  }
}

static void RunLoadAndDumpStats(size_t number_of_threads,
                                size_t number_of_requests, size_t input_size) {
  auto exec_times = vector<vector<uint64_t>>(number_of_threads);

  Stopwatch timer;
  timer.Start();
  RunLoad(number_of_requests, number_of_threads, input_size, &exec_times);
  auto elapsed_time_sec = duration_cast<seconds>(timer.Stop()).count();
  if (elapsed_time_sec == 0) {
    elapsed_time_sec = 1;
  }
  std::cout << "Throughput: " << number_of_requests / elapsed_time_sec
            << " requests per second" << std::endl;

  DumpStats({50, 90, 95}, exec_times);
}

static void RunTest(size_t num_workers_and_threads) {
  Config config;
  config.number_of_workers = num_workers_and_threads;
  auto status = RomaInit(config);
  EXPECT_TRUE(status.ok());
  LoadCode();

  for (size_t input_size = 0; input_size <= 1000000; input_size += 100000) {
    std::cout << "Run with " << num_workers_and_threads << " worker(s),"
              << num_workers_and_threads
              << " thread(s) sending requests, and input size " << input_size
              << " bytes" << std::endl;
    RunLoadAndDumpStats(num_workers_and_threads /*number_of_threads*/,
                        10000 /*number_of_requests*/, input_size);
  }

  status = RomaStop();
  EXPECT_TRUE(status.ok());
}

// One Roma worker with one thread sending 10K request.
// Payload varies from 0 bytes to 1M bytes in 100K increments.
TEST(RomaBenchmarkTest, OneWorkerTenThousandRequests) {
  auto num_workers_and_threads = 1;
  RunTest(num_workers_and_threads);
}

// Five Roma worker with five threads sending 10K request.
// Payload varies from 0 bytes to 1M bytes in 100K increments.
TEST(RomaBenchmarkTest, FiveWorkersTenThousandRequests) {
  auto num_workers_and_threads = 5;
  RunTest(num_workers_and_threads);
}

// Ten Roma worker with then threads sending 10K request.
// Payload varies from 0 bytes to 1M bytes in 100K increments.
TEST(RomaBenchmarkTest, TenWorkersTenThousandRequests) {
  auto num_workers_and_threads = 10;
  RunTest(num_workers_and_threads);
}
}  // namespace google::scp::roma::test
