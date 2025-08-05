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

#include "cc/core/common/auto_expiry_concurrent_map/src/auto_expiry_concurrent_map.h"

#include <gtest/gtest.h>

#include <math.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cc/core/async_executor/mock/mock_async_executor.h"
#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/common/auto_expiry_concurrent_map/mock/mock_auto_expiry_concurrent_map.h"
#include "cc/core/common/time_provider/src/time_provider.h"
#include "cc/core/test/utils/conditional_wait.h"
#include "cc/core/test/utils/error_codes.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

namespace privacy_sandbox::pbs_common {

class EmptyEntry {};

using UnderlyingEntry = AutoExpiryConcurrentMap<
    int, std::shared_ptr<EmptyEntry>,
    oneapi::tbb::tbb_hash_compare<int>>::AutoExpiryConcurrentMapEntry;

class AutoExpiryConcurrentMapTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_async_executor_ = std::make_shared<MockAsyncExecutor>();
    mock_async_executor_->schedule_for_mock =
        [&](const AsyncOperation& work, Timestamp, std::function<bool()>&) {
          return SuccessExecutionResult();
        };
  }

  size_t cache_lifetime_ = 10;
  std::shared_ptr<MockAsyncExecutor> mock_async_executor_;
  std::function<void(int&, std::shared_ptr<EmptyEntry>&,
                     std::function<void(bool)>)>
      on_before_element_deletion_callback_ =
          [](int& key, std::shared_ptr<EmptyEntry>&,
             std::function<void(bool can_delete)> deleter) {};
};

TEST_F(AutoExpiryConcurrentMapTest, InsertingNewElementExtendOnAccessEnabled) {
  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, true, true, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Run());
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));
  EXPECT_THAT(
      auto_expiry_map.Insert(pair, entry),
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)));

  std::shared_ptr<UnderlyingEntry> underlying_entry;
  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);

  underlying_entry->being_evicted = true;
  uint64_t current_expiration = underlying_entry->expiration_time.load();
  EXPECT_NE(current_expiration, 0);

  EXPECT_THAT(auto_expiry_map.Insert(pair, entry),
              ResultIs(FailureExecutionResult(
                  SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED)));

  underlying_entry->being_evicted = false;

  auto current_clock = (TimeProvider::GetSteadyTimestampInNanoseconds() +
                        std::chrono::seconds(cache_lifetime_))
                           .count();

  EXPECT_THAT(
      auto_expiry_map.Insert(pair, entry),
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)));

  EXPECT_GE(underlying_entry->expiration_time.load(), current_clock);
}

TEST_F(AutoExpiryConcurrentMapTest,
       InsertingNewElementExtendOnAccessEnabledBlockingDisabled) {
  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, true, false, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Run());
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));
  EXPECT_THAT(
      auto_expiry_map.Insert(pair, entry),
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)));

  std::shared_ptr<UnderlyingEntry> underlying_entry;
  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);

  underlying_entry->being_evicted = true;
  uint64_t current_expiration = underlying_entry->expiration_time.load();
  EXPECT_NE(current_expiration, 0);

  EXPECT_THAT(
      auto_expiry_map.Insert(pair, entry),
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)));

  underlying_entry->being_evicted = false;

  auto current_clock = (TimeProvider::GetSteadyTimestampInNanoseconds() +
                        std::chrono::seconds(cache_lifetime_))
                           .count();

  EXPECT_THAT(
      auto_expiry_map.Insert(pair, entry),
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)));

  EXPECT_GE(underlying_entry->expiration_time.load(), current_clock);
}

TEST_F(AutoExpiryConcurrentMapTest, InsertingNewElementExtendOnAccessDisabled) {
  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, false, true, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Run());
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  std::shared_ptr<UnderlyingEntry> underlying_entry;
  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);

  uint64_t current_expiration = underlying_entry->expiration_time.load();
  EXPECT_NE(current_expiration, 0);

  EXPECT_THAT(
      auto_expiry_map.Insert(pair, entry),
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)));

  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);
  EXPECT_EQ(underlying_entry->expiration_time.load(), current_expiration);
}

TEST_F(AutoExpiryConcurrentMapTest,
       InsertingNewElementExtendOnAccessDisabledBlockingDisabled) {
  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, false, false, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Run());
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  std::shared_ptr<UnderlyingEntry> underlying_entry;
  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);

  uint64_t current_expiration = underlying_entry->expiration_time.load();
  EXPECT_NE(current_expiration, 0);

  EXPECT_THAT(
      auto_expiry_map.Insert(pair, entry),
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)));

  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);
  EXPECT_EQ(underlying_entry->expiration_time.load(), current_expiration);
}

TEST_F(AutoExpiryConcurrentMapTest, FindingElementExtendOnAccessEnabled) {
  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, true, true, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Run());
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  EXPECT_SUCCESS(auto_expiry_map.Find(3, entry));

  std::shared_ptr<UnderlyingEntry> underlying_entry;
  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);

  underlying_entry->being_evicted = true;
  EXPECT_THAT(auto_expiry_map.Find(3, entry),
              ResultIs(FailureExecutionResult(
                  SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED)));

  uint64_t current_expiration = underlying_entry->expiration_time.load();
  EXPECT_NE(current_expiration, 0);
  underlying_entry->being_evicted = false;

  auto current_clock = (TimeProvider::GetSteadyTimestampInNanoseconds() +
                        std::chrono::seconds(cache_lifetime_))
                           .count();
  EXPECT_SUCCESS(auto_expiry_map.Find(3, entry));

  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);
  EXPECT_GE(underlying_entry->expiration_time.load(), current_clock);
}

TEST_F(AutoExpiryConcurrentMapTest,
       FindingElementExtendOnAccessEnabledBlockingDisabled) {
  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, true, false, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Run());
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  EXPECT_SUCCESS(auto_expiry_map.Find(3, entry));

  std::shared_ptr<UnderlyingEntry> underlying_entry;
  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);

  underlying_entry->being_evicted = true;
  EXPECT_SUCCESS(auto_expiry_map.Find(3, entry));

  uint64_t current_expiration = underlying_entry->expiration_time.load();
  EXPECT_NE(current_expiration, 0);
  underlying_entry->being_evicted = false;

  auto current_clock = (TimeProvider::GetSteadyTimestampInNanoseconds() +
                        std::chrono::seconds(cache_lifetime_))
                           .count();
  EXPECT_SUCCESS(auto_expiry_map.Find(3, entry));

  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);
  EXPECT_GE(underlying_entry->expiration_time.load(), current_clock);
}

TEST_F(AutoExpiryConcurrentMapTest, FindingElementExtendOnAccessDisabled) {
  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, false, true, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Run());
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  std::shared_ptr<UnderlyingEntry> underlying_entry;
  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);

  uint64_t current_expiration = underlying_entry->expiration_time.load();
  EXPECT_NE(current_expiration, 0);

  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);
  EXPECT_EQ(underlying_entry->expiration_time.load(), current_expiration);
}

TEST_F(AutoExpiryConcurrentMapTest,
       FindingElementExtendOnAccessDisabledBlockingDisabled) {
  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, false, false, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Run());
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  std::shared_ptr<UnderlyingEntry> underlying_entry;
  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);

  uint64_t current_expiration = underlying_entry->expiration_time.load();
  EXPECT_NE(current_expiration, 0);

  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);
  EXPECT_EQ(underlying_entry->expiration_time.load(), current_expiration);
}

TEST_F(AutoExpiryConcurrentMapTest, Erase) {
  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, false, true, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Run());
  EXPECT_THAT(
      auto_expiry_map.Erase(pair.first),
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));
  EXPECT_SUCCESS(auto_expiry_map.Erase(pair.first));
}

TEST_F(AutoExpiryConcurrentMapTest, GetKeys) {
  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, false, true, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  EXPECT_SUCCESS(auto_expiry_map.Run());

  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));
  pair = make_pair(4, entry);
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  std::vector<int> keys;
  EXPECT_SUCCESS(auto_expiry_map.Keys(keys));
  EXPECT_EQ(keys.size(), 2);
  EXPECT_NE(std::find(keys.begin(), keys.end(), 3), keys.end());
  EXPECT_NE(std::find(keys.begin(), keys.end(), 4), keys.end());
}

TEST_F(AutoExpiryConcurrentMapTest, DisableEviction) {
  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, true, true, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  EXPECT_SUCCESS(auto_expiry_map.Run());
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  std::shared_ptr<UnderlyingEntry> underlying_entry;
  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);

  EXPECT_EQ(underlying_entry->is_evictable, true);
  EXPECT_SUCCESS(auto_expiry_map.DisableEviction(3));
  EXPECT_EQ(underlying_entry->is_evictable, false);

  EXPECT_THAT(
      auto_expiry_map.DisableEviction(5),
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));

  underlying_entry->being_evicted = true;
  EXPECT_THAT(auto_expiry_map.DisableEviction(3),
              ResultIs(FailureExecutionResult(
                  SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED)));
}

TEST_F(AutoExpiryConcurrentMapTest, EnableEviction) {
  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, true, true, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  EXPECT_SUCCESS(auto_expiry_map.Run());
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  std::shared_ptr<UnderlyingEntry> underlying_entry;
  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);

  EXPECT_EQ(underlying_entry->is_evictable, true);
  underlying_entry->is_evictable = false;
  EXPECT_EQ(underlying_entry->is_evictable, false);

  EXPECT_SUCCESS(auto_expiry_map.EnableEviction(3));
  EXPECT_EQ(underlying_entry->is_evictable, true);

  EXPECT_THAT(
      auto_expiry_map.EnableEviction(5),
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));

  underlying_entry->being_evicted = true;
  EXPECT_THAT(auto_expiry_map.EnableEviction(3),
              ResultIs(FailureExecutionResult(
                  SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED)));
}

TEST_F(AutoExpiryConcurrentMapTest, GarbageCollection) {
  std::vector<int> keys_to_be_deleted;
  auto on_before_element_deletion_callback_ =
      [&](int& key, std::shared_ptr<EmptyEntry>&,
          std::function<void(bool can_delete)> deleter) {
        keys_to_be_deleted.push_back(key);
      };

  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, true, true, on_before_element_deletion_callback_,
      mock_async_executor_);

  EXPECT_SUCCESS(auto_expiry_map.Run());

  auto entry = std::make_shared<EmptyEntry>();
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  pair = make_pair(4, entry);
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  std::shared_ptr<UnderlyingEntry> underlying_entry;
  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);
  underlying_entry->expiration_time = 0;

  std::shared_lock<std::shared_timed_mutex> lock(underlying_entry->record_lock);

  auto_expiry_map.RunGarbageCollector();
  EXPECT_EQ(keys_to_be_deleted.size(), 0);

  lock.unlock();
  underlying_entry->is_evictable = false;

  auto_expiry_map.RunGarbageCollector();
  EXPECT_EQ(keys_to_be_deleted.size(), 0);

  underlying_entry->expiration_time = UINT64_MAX;
  auto_expiry_map.RunGarbageCollector();
  EXPECT_EQ(keys_to_be_deleted.size(), 0);

  underlying_entry->expiration_time = 0;
  underlying_entry->is_evictable = true;
  auto_expiry_map.RunGarbageCollector();
  EXPECT_EQ(keys_to_be_deleted.size(), 1);
  EXPECT_EQ(keys_to_be_deleted[0], 3);
}

TEST_F(AutoExpiryConcurrentMapTest, OnRemoveEntryFromCacheLogged) {
  std::vector<int> keys_to_be_deleted;
  auto on_before_element_deletion_callback_ =
      [&](int& key, std::shared_ptr<EmptyEntry>&,
          std::function<void(bool can_delete)> deleter) {
        keys_to_be_deleted.push_back(key);
      };

  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      cache_lifetime_, true, true, on_before_element_deletion_callback_,
      mock_async_executor_);

  EXPECT_SUCCESS(auto_expiry_map.Run());

  auto entry = std::make_shared<EmptyEntry>();
  auto pair = make_pair(3, entry);
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  pair = make_pair(4, entry);
  EXPECT_SUCCESS(auto_expiry_map.Insert(pair, entry));

  std::shared_ptr<UnderlyingEntry> underlying_entry;
  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);

  std::pair<
      int,
      std::shared_ptr<AutoExpiryConcurrentMap<
          int, std::shared_ptr<EmptyEntry>,
          oneapi::tbb::tbb_hash_compare<int>>::AutoExpiryConcurrentMapEntry>>
      map_pair = std::make_pair(3, underlying_entry);
  auto_expiry_map.OnRemoveEntryFromCacheLogged(map_pair, true);
  EXPECT_THAT(
      auto_expiry_map.Find(3, entry),
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));

  map_pair = std::make_pair(4, underlying_entry);
  auto_expiry_map.OnRemoveEntryFromCacheLogged(map_pair, false);
  EXPECT_SUCCESS(auto_expiry_map.Find(4, entry));
}

TEST_F(AutoExpiryConcurrentMapTest, Run) {
  std::vector<ExecutionResult> results = {SuccessExecutionResult(),
                                          FailureExecutionResult(123),
                                          RetryExecutionResult(123)};

  for (auto result : results) {
    mock_async_executor_->schedule_for_mock =
        [&](const AsyncOperation& work, Timestamp, std::function<bool()>&) {
          return result;
        };

    AutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
        10, true, true, on_before_element_deletion_callback_,
        mock_async_executor_);

    EXPECT_THAT(auto_expiry_map.Run(), ResultIs(result));
  }
}

TEST_F(AutoExpiryConcurrentMapTest, NoDeletionForUnloadedData) {
  mock_async_executor_->schedule_mock = [&](const AsyncOperation& work) {
    work();
    return SuccessExecutionResult();
  };

  bool schedule_for_is_called = false;
  mock_async_executor_->schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp timestamp,
          std::function<bool()>& cancellation_callback) {
        schedule_for_is_called = true;
        return SuccessExecutionResult();
      };

  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      0, true, true, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  auto pair = make_pair(3, entry);
  std::shared_ptr<UnderlyingEntry> underlying_entry =
      std::make_shared<UnderlyingEntry>(entry, 0);
  auto underlying_pair = make_pair(3, underlying_entry);
  auto_expiry_map.GetUnderlyingConcurrentMap().Insert(underlying_pair,
                                                      underlying_entry);
  underlying_entry->is_evictable = false;
  EXPECT_SUCCESS(auto_expiry_map.Run());

  auto_expiry_map.GetUnderlyingConcurrentMap().Find(3, underlying_entry);

  WaitUntil([&]() { return schedule_for_is_called; });

  EXPECT_SUCCESS(auto_expiry_map.Find(3, entry));
}

TEST_F(AutoExpiryConcurrentMapTest, NoDeletionForUnExpired) {
  mock_async_executor_->schedule_mock = [&](const AsyncOperation& work) {
    work();
    return SuccessExecutionResult();
  };

  bool schedule_for_is_called = false;
  mock_async_executor_->schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp timestamp,
          std::function<bool()>& cancellation_callback) {
        schedule_for_is_called = true;
        return SuccessExecutionResult();
      };

  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      0, true, true, on_before_element_deletion_callback_,
      mock_async_executor_);

  auto entry = std::make_shared<EmptyEntry>();
  std::shared_ptr<UnderlyingEntry> underlying_entry =
      std::make_shared<UnderlyingEntry>(entry, 0);
  auto underlying_pair = make_pair(3, underlying_entry);
  auto_expiry_map.GetUnderlyingConcurrentMap().Insert(underlying_pair,
                                                      underlying_entry);
  underlying_entry->expiration_time = 999999999999999999;

  EXPECT_SUCCESS(auto_expiry_map.Run());

  WaitUntil([&]() { return schedule_for_is_called; });

  EXPECT_SUCCESS(auto_expiry_map.Find(3, entry));
}

TEST(AutoExpiryConcurrentMapDeletionTest, DeletionForExpired) {
  size_t total_count = 0;
  std::vector<std::function<void(bool)>> deleters;
  auto on_before_element_deletion_callback =
      [&](int& key, std::shared_ptr<EmptyEntry>& entry,
          std::function<void(bool)> deleter) {
        total_count++;
        deleters.push_back(deleter);
      };

  auto mock_async_executor = std::make_shared<MockAsyncExecutor>();

  MockAutoExpiryConcurrentMap<int, std::shared_ptr<EmptyEntry>> auto_expiry_map(
      0, true, true, on_before_element_deletion_callback, mock_async_executor);

  auto entry = std::make_shared<EmptyEntry>();
  std::shared_ptr<UnderlyingEntry> underlying_entry =
      std::make_shared<UnderlyingEntry>(entry, 0);
  auto underlying_pair = make_pair(3, underlying_entry);
  auto_expiry_map.GetUnderlyingConcurrentMap().Insert(underlying_pair,
                                                      underlying_entry);
  underlying_entry->expiration_time = 0;
  underlying_entry->is_evictable = true;

  entry = std::make_shared<EmptyEntry>();
  underlying_pair = make_pair(5, underlying_entry);
  auto_expiry_map.GetUnderlyingConcurrentMap().Insert(underlying_pair,
                                                      underlying_entry);
  underlying_entry->expiration_time = 0;
  underlying_entry->is_evictable = true;
  EXPECT_SUCCESS(auto_expiry_map.Run());

  WaitUntil([&]() { return total_count == 2; });

  EXPECT_THAT(auto_expiry_map.Find(3, entry),
              ResultIs(FailureExecutionResult(
                  SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED)));
  EXPECT_THAT(auto_expiry_map.Find(5, entry),
              ResultIs(FailureExecutionResult(
                  SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED)));

  deleters[0](true);

  bool schedule_for_called = false;
  mock_async_executor->schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp, std::function<bool()>&) {
        schedule_for_called = true;
        return SuccessExecutionResult();
      };

  deleters[1](false);

  EXPECT_THAT(
      auto_expiry_map.Find(3, entry),
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));
  EXPECT_SUCCESS(auto_expiry_map.Find(5, entry));

  WaitUntil([&]() { return schedule_for_called; });
}

TEST(AutoExpiryConcurrentMapEntryTest, ExtendEntryExpiration) {
  std::shared_ptr<EmptyEntry> empty_entry;
  UnderlyingEntry entry(empty_entry, 1);
  entry.expiration_time = 0;
  EXPECT_EQ(entry.IsExpired(), true);

  EXPECT_THAT(entry.ExtendExpiration(0),
              ResultIs(FailureExecutionResult(
                  SC_AUTO_EXPIRY_CONCURRENT_MAP_INVALID_EXPIRATION)));

  auto current_time =
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();
  EXPECT_SUCCESS(entry.ExtendExpiration(10));
  EXPECT_GE(entry.expiration_time.load(), current_time);

  entry.being_evicted = true;
  auto cached_expiration = entry.expiration_time.load();
  EXPECT_THAT(entry.ExtendExpiration(10),
              ResultIs(FailureExecutionResult(
                  SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED)));
  EXPECT_EQ(cached_expiration, entry.expiration_time.load());
}

TEST(AutoExpiryConcurrentMapEntryTest, IsEntryExpired) {
  std::shared_ptr<EmptyEntry> empty_entry;
  UnderlyingEntry entry(empty_entry, 1);
  entry.expiration_time = UINT64_MAX;
  EXPECT_EQ(entry.IsExpired(), false);

  entry.expiration_time = 0;
  EXPECT_EQ(entry.IsExpired(), true);

  auto current_time = TimeProvider::GetSteadyTimestampInNanoseconds();
  entry.expiration_time = current_time.count();
  EXPECT_EQ(entry.IsExpired(), true);

  entry.expiration_time =
      (current_time + std::chrono::seconds(2)).count(); /* adding 2 seconds */
  EXPECT_EQ(entry.IsExpired(), false);
}

TEST(AutoExpiryConcurrentMapEntryTest, StopShouldWaitForScheduledWork) {
  std::mutex gc_completion_lambdas_mutex;
  std::vector<std::function<void(bool)>> gc_completion_lambdas;
  auto async_executor = std::make_shared<AsyncExecutor>(/*thread_count=*/4,
                                                        /*queue_capacity=*/100);

  // Do not invoke the completion lambdas yet, but collect them and invoke them
  // later.
  auto on_before_gc_lambda = [&gc_completion_lambdas,
                              &gc_completion_lambdas_mutex](
                                 std::string&, std::shared_ptr<std::string>&,
                                 std::function<void(bool)> completion_lambda) {
    std::unique_lock lock(gc_completion_lambdas_mutex);
    gc_completion_lambdas.push_back(std::move(completion_lambda));
  };

  AutoExpiryConcurrentMap<std::string, std::shared_ptr<std::string>> map(
      /*evict_timeout=*/1, /*extend_entry_lifetime_on_access=*/false,
      /*block_entry_while_eviction=*/false, on_before_gc_lambda,
      async_executor);

  // Insert 3 entries. 3 callback lambdas will be invoked once the garbage
  // collection work is started.
  {
    std::shared_ptr<std::string> out_value;
    EXPECT_SUCCESS(map.Insert(
        make_pair("key1", std::make_shared<std::string>("value1")), out_value));
  }
  {
    std::shared_ptr<std::string> out_value;
    EXPECT_SUCCESS(map.Insert(
        make_pair("key2", std::make_shared<std::string>("value2")), out_value));
  }
  {
    std::shared_ptr<std::string> out_value;
    EXPECT_SUCCESS(map.Insert(
        make_pair("key3", std::make_shared<std::string>("value3")), out_value));
  }

  EXPECT_SUCCESS(async_executor->Init());
  EXPECT_SUCCESS(async_executor->Run());

  EXPECT_SUCCESS(map.Init());
  EXPECT_SUCCESS(map.Run());

  WaitUntil([&gc_completion_lambdas, &gc_completion_lambdas_mutex]() {
    std::unique_lock lock(gc_completion_lambdas_mutex);
    return gc_completion_lambdas.size() == 3;
  });

  // Stopper thread. Start stoping the map...
  std::atomic<bool> stop_completed = false;
  std::thread stop_thread([&map, &stop_completed]() {
    EXPECT_SUCCESS(map.Stop());
    stop_completed = true;
  });

  // Cannot be stopped because there is pending work..
  EXPECT_THAT(
      WaitUntilOrReturn([&stop_completed]() { return stop_completed.load(); },
                        /*timeout=*/std::chrono::milliseconds(2000)),
      ResultIs(FailureExecutionResult(SC_TEST_UTILS_TEST_WAIT_TIMEOUT)));

  // Invoke the lambdas to complete the pending work, and unblocking the stopper
  // thread.
  for (auto& lambda : gc_completion_lambdas) {
    lambda(true);
  }

  EXPECT_SUCCESS(
      WaitUntilOrReturn([&stop_completed]() { return stop_completed.load(); },
                        /*timeout=*/std::chrono::milliseconds(2000)));

  stop_thread.join();
  EXPECT_SUCCESS(async_executor->Stop());
}
}  // namespace privacy_sandbox::pbs_common
