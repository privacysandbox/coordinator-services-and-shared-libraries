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

#include "core/common/time_provider/src/time_provider.h"

#include <gtest/gtest.h>

#include <sys/time.h>

#include <thread>

using std::chrono::nanoseconds;

namespace google::scp::core::common::test {
TEST(TimeProviderTest,
     GetUniqueWallTimestampInNanosecondsReturnsUniqueTimestampSingleThread) {
  nanoseconds prev_timestamp =
      TimeProvider::GetUniqueWallTimestampInNanoseconds();
  for (int i = 0; i < 100000; i++) {
    auto timestamp = TimeProvider::GetUniqueWallTimestampInNanoseconds();
    EXPECT_NE(timestamp.count(), 0);
    EXPECT_GT(timestamp, prev_timestamp);
    prev_timestamp = timestamp;
  }
}

// This test should be enabled when we have a better implementation for the
// GetUniqueWallTimestampInNanoseconds to guarantee unique timestamps across
// threads.
TEST(TimeProviderTest,
     GetUniqueWallTimestampInNanosecondsReturnsUniqueTimestampMultiThread) {
  const int thread_count = 10;
  const int timestamp_fetch_count = 1000;

  std::vector<std::thread> threads;
  std::atomic<bool> start(false);
  std::vector<std::chrono::nanoseconds> thread_fetched_timestamps[thread_count];

  for (int i = 0; i < thread_count; i++) {
    threads.emplace_back(
        [&](int thread_number) {
          while (!start.load()) {}

          nanoseconds prev_timestamp =
              TimeProvider::GetUniqueWallTimestampInNanoseconds();
          for (int i = 0; i < timestamp_fetch_count; i++) {
            auto timestamp =
                TimeProvider::GetUniqueWallTimestampInNanoseconds();
            EXPECT_GT(timestamp, prev_timestamp);
            EXPECT_GT(timestamp.count(), 0);
            prev_timestamp = timestamp;
            thread_fetched_timestamps[thread_number].push_back(timestamp);
          }
        },
        i);
  }

  start.store(true);

  for (int i = 0; i < thread_count; i++) {
    threads[i].join();
  }

  // Check uniqueness of the timestamps fetched by all the threads.

  std::vector<std::chrono::nanoseconds> all_thread_fetched_timestamps;
  for (int i = 0; i < thread_count; i++) {
    for (auto timestamp_val : thread_fetched_timestamps[i]) {
      all_thread_fetched_timestamps.push_back(timestamp_val);
    }
  }

  for (size_t i = 0; i < all_thread_fetched_timestamps.size(); i++) {
    for (size_t j = i + 1; j < all_thread_fetched_timestamps.size(); j++) {
      EXPECT_NE(all_thread_fetched_timestamps[i],
                all_thread_fetched_timestamps[j]);
    }
  }
}
};  // namespace google::scp::core::common::test
