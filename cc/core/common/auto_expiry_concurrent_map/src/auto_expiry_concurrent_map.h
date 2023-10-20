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

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

static constexpr char kAutoExpiryConcurrentMap[] = "AutoExpiryConcurrentMap";

static constexpr std::chrono::milliseconds
    kAutoExpiryConcurrentMapStopWaitSleepDuration =
        std::chrono::milliseconds(100);

static constexpr std::chrono::seconds
    kAutoExpiryConcurrentMapStopWaitMaxDurationToWait =
        std::chrono::seconds(10);

namespace google::scp::core::common {
/**
 * @brief AutoExpiryConcurrentMap provides auto cleanup functionality on
 * top of a concurrent map which is a multi producers and multi consumers map
 * support to be used generically.
 */
template <class TKey, class TValue,
          typename TCompare = oneapi::tbb::tbb_hash_compare<TKey>>
class AutoExpiryConcurrentMap : public ServiceInterface {
 public:
  /**
   * @brief Represents auto expiry concurrent map entry. Any value in the
   * map must have the following two properties.
   */
  struct AutoExpiryConcurrentMapEntry {
    AutoExpiryConcurrentMapEntry(TValue& entry, size_t expiration_in_seconds)
        : entry(entry), being_evicted(false), is_evictable(true) {
      expiration_time = (TimeProvider::GetSteadyTimestampInNanoseconds() +
                         std::chrono::seconds(expiration_in_seconds))
                            .count();
    }

    /**
     * @brief Extends the expiration time of the map entry.
     *
     * @param seconds The total seconds the current entry can be extended. The
     * given parameter value must be greater than 0.
     * @return ExecutionResult The execution result of the operation.
     */
    ExecutionResult ExtendExpiration(size_t expiration_seconds) {
      if (expiration_seconds == 0) {
        return FailureExecutionResult(
            errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_INVALID_EXPIRATION);
      }

      std::shared_lock<std::shared_timed_mutex> lock(record_lock);

      if (being_evicted) {
        return FailureExecutionResult(
            errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED);
      }

      expiration_time = (TimeProvider::GetSteadyTimestampInNanoseconds() +
                         std::chrono::seconds(expiration_seconds))
                            .count();

      return SuccessExecutionResult();
    }

    /**
     * @brief Returns true if the current entry is already expired.
     *
     * @return true If the entry is expired.
     * @return false If the entry is not expired.
     */
    bool IsExpired() {
      auto current_time =
          TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();

      return expiration_time < current_time;
    }

    /**
     * @brief Locks the record and gets exclusive access. After the write is
     * acquired, the expiration will be ignored until the lock is released.
     */
    std::shared_timed_mutex record_lock;

    /// An instance of the actual entry of the map.
    TValue entry;

    /// Indicates if the entry is being deleted.
    bool being_evicted;

    /// Indicates if the entry is evictable.
    bool is_evictable;

    /// Expiration of the entry in the memory
    std::atomic<core::Timestamp> expiration_time;
  };

  /**
   * @brief Construct a new auto expiry concurrent map object
   *
   * @param map_entry_lifetime_seconds The lifetime of the map entry in
   * seconds. After the time passes, the element will be removed from the map.
   * @param extend_entry_lifetime_on_access Extends the map will extend the
   * entries lifetime on access.
   * @param block_entry_while_eviction Blocks the entry from access while the
   * eviction operation is in process.
   * @param on_before_element_deletion_callback The callback to be called
   * right before removing the element from the map.
   * @param async_executor An instance to the async executor.
   */
  AutoExpiryConcurrentMap(
      size_t map_entry_lifetime_seconds, bool extend_entry_lifetime_on_access,
      bool block_entry_while_eviction,
      std::function<void(TKey&, TValue&, std::function<void(bool)>)>
          on_before_element_deletion_callback,
      const std::shared_ptr<AsyncExecutorInterface>& async_executor)
      : map_entry_lifetime_seconds_(map_entry_lifetime_seconds),
        extend_entry_lifetime_on_access_(extend_entry_lifetime_on_access),
        block_entry_while_eviction_(block_entry_while_eviction),
        on_before_element_deletion_callback_(
            on_before_element_deletion_callback),
        async_executor_(async_executor),
        pending_garbage_collection_callbacks_(0),
        is_running_(false) {}

  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept override {
    is_running_ = true;
    return ScheduleGarbageCollection();
  }

  ExecutionResult Stop() noexcept override {
    SCP_INFO(kAutoExpiryConcurrentMap, kZeroUuid, "Stopping...");
    sync_mutex.lock();
    is_running_ = false;
    sync_mutex.unlock();

    current_cancellation_callback_();

    // Wait until scheduled work (if any) is completed
    auto wait_start_timestamp = TimeProvider::GetSteadyTimestampInNanoseconds();
    while (pending_garbage_collection_callbacks_ > 0) {
      std::this_thread::sleep_for(
          kAutoExpiryConcurrentMapStopWaitSleepDuration);
      // If timeout, then return an error.
      if ((TimeProvider::GetSteadyTimestampInNanoseconds() -
           wait_start_timestamp) >=
          kAutoExpiryConcurrentMapStopWaitMaxDurationToWait) {
        auto result = FailureExecutionResult(
            core::errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_STOP_INCOMPLETE);
        SCP_ERROR(
            kAutoExpiryConcurrentMap, kZeroUuid, result,
            "Exiting prematurely. Waited for '%llu' (ms). There are still "
            "'%llu' pending callbacks.",
            std::chrono::duration_cast<std::chrono::milliseconds>(
                TimeProvider::GetSteadyTimestampInNanoseconds() -
                wait_start_timestamp)
                .count(),
            pending_garbage_collection_callbacks_.load());
        return result;
      }
    }

    return SuccessExecutionResult();
  }

  /**
   * @brief Inserts an element into a concurrent map. The caller must provider
   * the key value pair to be inserted into the map. However due to concurrency
   * nature of the structure, there is a chance that another caller is inserting
   * the same key. If the execution result of the operation is success, it means
   * the caller thread has been the first thread that could insert this key and
   * value into the map otherwise, out_key will point to he existing key in the
   * map and caller must use that value.
   *
   * @param key_value A pair of key value containing the key and values to be
   * inserted.
   * @param out_value A reference to the actual value inserted into the map.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult Insert(std::pair<TKey, TValue> key_value,
                                 TValue& out_value) noexcept {
    auto record = std::make_shared<AutoExpiryConcurrentMapEntry>(
        key_value.second, map_entry_lifetime_seconds_);

    auto pair = std::make_pair(key_value.first, record);
    auto execution_result = concurrent_map_.Insert(pair, record);

    if (!execution_result.Successful()) {
      if (execution_result !=
          FailureExecutionResult(
              core::errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)) {
        return execution_result;
      }

      std::shared_lock<std::shared_timed_mutex> lock(record->record_lock);
      if (block_entry_while_eviction_ && record->being_evicted) {
        return FailureExecutionResult(
            errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED);
      }

      if (extend_entry_lifetime_on_access_ && !record->being_evicted) {
        record->ExtendExpiration(map_entry_lifetime_seconds_);
      }
    }

    out_value = record->entry;
    return execution_result;
  }

  /**
   * @brief Finds an element within the map with the provided key. If the key
   * does not exist the find operation will return a proper error code to the
   * caller thread.
   *
   * @param key The key to be found from the map.
   * @param out_value A reference to the actual value in the map.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult Find(const TKey& key, TValue& out_value) noexcept {
    std::shared_ptr<AutoExpiryConcurrentMapEntry> record;
    auto execution_result = concurrent_map_.Find(key, record);

    if (execution_result.Successful()) {
      std::shared_lock<std::shared_timed_mutex> lock(record->record_lock);
      if (block_entry_while_eviction_ && record->being_evicted) {
        return FailureExecutionResult(
            errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED);
      }

      if (extend_entry_lifetime_on_access_ && !record->being_evicted) {
        record->ExtendExpiration(map_entry_lifetime_seconds_);
      }

      out_value = record->entry;
    }
    return execution_result;
  }

  /**
   * @brief Erases an element from the map with the provided key. If the key
   * does not exist the erase operation will return a proper error code to the
   * caller thread.
   *
   * @param key The key to be erased from the map.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult Erase(TKey& key) noexcept {
    return concurrent_map_.Erase(key);
  }

  /**
   * @brief Gets all the keys in the current concurrent map. This is a very
   * expensive operation due to locking.
   *
   * @param keys A vector of the keys to be filled in once looked up.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult Keys(std::vector<TKey>& keys) noexcept {
    return concurrent_map_.Keys(keys);
  }

  /**
   * @brief Returns the count of elements present in the map
   *
   * @return size_t count of elements
   */
  size_t Size() noexcept { return concurrent_map_.Size(); }

  /**
   * @brief Prevents evicting an element in the map provided by the key.
   *
   * @param key The key to be used to find the element to disable eviction.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult DisableEviction(const TKey& key) noexcept {
    std::shared_ptr<AutoExpiryConcurrentMapEntry> record;
    auto execution_result = concurrent_map_.Find(key, record);
    if (execution_result.Successful()) {
      std::shared_lock<std::shared_timed_mutex> lock(record->record_lock);
      if (record->being_evicted) {
        return FailureExecutionResult(
            errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED);
      }

      if (extend_entry_lifetime_on_access_) {
        record->ExtendExpiration(map_entry_lifetime_seconds_);
      }

      record->is_evictable = false;
    }
    return execution_result;
  }

  /**
   * @brief Enables evicting an element in the map provided by the key.
   *
   * @param key The key to be used to find the element to enable eviction.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult EnableEviction(const TKey& key) noexcept {
    std::shared_ptr<AutoExpiryConcurrentMapEntry> record;
    auto execution_result = concurrent_map_.Find(key, record);
    if (execution_result.Successful()) {
      std::shared_lock<std::shared_timed_mutex> lock(record->record_lock);
      if (record->being_evicted) {
        return FailureExecutionResult(
            errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_ENTRY_BEING_DELETED);
      }

      if (extend_entry_lifetime_on_access_) {
        record->ExtendExpiration(map_entry_lifetime_seconds_);
      }

      record->is_evictable = true;
    }
    return execution_result;
  }

 protected:
  /**
   * @brief Schedules a round of garbage collection in the next
   * map_entry_lifetime_seconds_.
   */
  core::ExecutionResult ScheduleGarbageCollection() noexcept {
    Timestamp next_schedule_time =
        (TimeProvider::GetSteadyTimestampInNanoseconds() +
         std::chrono::seconds(map_entry_lifetime_seconds_))
            .count();
    sync_mutex.lock();
    if (!is_running_) {
      sync_mutex.unlock();
      return FailureExecutionResult(
          errors::SC_AUTO_EXPIRY_CONCURRENT_MAP_CANNOT_SCHEDULE);
    }

    auto execution_result = async_executor_->ScheduleFor(
        [this]() { RunGarbageCollector(); }, next_schedule_time,
        current_cancellation_callback_);
    if (!execution_result.Successful()) {
      // TODO: Create an alert
    }

    sync_mutex.unlock();
    return execution_result;
  }

  /**
   * @brief Runs the actual garbage collection logic. This operation must be
   * error free to avoid memory increases overtime. In the case of errors an
   * alert must be raised.
   */
  void RunGarbageCollector() {
    std::vector<TKey> keys;
    auto execution_result = concurrent_map_.Keys(keys);
    if (!execution_result.Successful()) {
      // Reschedule
      ScheduleGarbageCollection();
      return;
    }

    std::vector<std::pair<TKey, std::shared_ptr<AutoExpiryConcurrentMapEntry>>>
        elements_to_remove;

    for (auto key : keys) {
      std::shared_ptr<AutoExpiryConcurrentMapEntry> value;
      auto execution_result = concurrent_map_.Find(key, value);
      if (!execution_result.Successful()) {
        // TODO: log and continue
        continue;
      }

      std::unique_lock<std::shared_timed_mutex> lock(value->record_lock,
                                                     std::defer_lock);
      if (!lock.try_lock()) {
        continue;
      }

      if (!value->is_evictable || !value->IsExpired()) {
        continue;
      }

      value->being_evicted = true;
      elements_to_remove.push_back(std::make_pair(key, value));
    }

    if (elements_to_remove.size() == 0) {
      ScheduleGarbageCollection();
      return;
    }

    pending_garbage_collection_callbacks_ = elements_to_remove.size();

    for (auto element_to_remove : elements_to_remove) {
      auto key = std::get<0>(element_to_remove);
      auto value = std::get<1>(element_to_remove);
      auto callback =
          std::bind(&AutoExpiryConcurrentMap::OnRemoveEntryFromCacheLogged,
                    this, element_to_remove, std::placeholders::_1);
      on_before_element_deletion_callback_(key, value->entry, callback);
    }
  }

  /**
   * @brief This is called when the element is ready to be deleted.
   *
   * @param key_value_pair The key value pair to be removed.
   * @param can_delete Indicates whether this element can be removed from the
   * map.
   */
  void OnRemoveEntryFromCacheLogged(
      std::pair<TKey, std::shared_ptr<AutoExpiryConcurrentMapEntry>>&
          key_value_pair,
      bool can_delete) noexcept {
    if (can_delete) {
      auto key = std::get<0>(key_value_pair);
      auto execution_result = concurrent_map_.Erase(key);
      if (!execution_result.Successful()) {
        // TODO: Log this.
        std::unique_lock<std::shared_timed_mutex> lock(
            std::get<1>(key_value_pair)->record_lock);
        std::get<1>(key_value_pair)->being_evicted = false;
      }
    } else {
      // TODO: Log.
      // Set the loaded flag to true since we dont want to keep it unavailable.
      std::unique_lock<std::shared_timed_mutex> lock(
          std::get<1>(key_value_pair)->record_lock);
      std::get<1>(key_value_pair)->being_evicted = false;
    }

    // Last callback
    if (pending_garbage_collection_callbacks_.fetch_sub(1) != 1) {
      return;
    }

    ScheduleGarbageCollection();
  }

  ConcurrentMap<TKey, std::shared_ptr<AutoExpiryConcurrentMapEntry>, TCompare>
      concurrent_map_;

 private:
  /// The map entry lifetime in seconds.
  const size_t map_entry_lifetime_seconds_;
  // Indicates whether to extend the entries lifetime on access.
  bool extend_entry_lifetime_on_access_;
  // Blocks the entry from access while the eviction operation is in process.
  bool block_entry_while_eviction_;
  /// The callback to be called right before removing an element from the map.
  std::function<void(TKey&, TValue&, std::function<void(bool)>)>
      on_before_element_deletion_callback_;
  /// An instance to the async executor.
  const std::shared_ptr<AsyncExecutorInterface> async_executor_;
  /// The total pending callbacks waiting during the garbage collection period.
  std::atomic<size_t> pending_garbage_collection_callbacks_;
  /// The cancellation callback.
  std::function<bool()> current_cancellation_callback_;
  /// Sync mutex
  std::mutex sync_mutex;
  /// Indicates whther the component stopped
  bool is_running_;
};
}  // namespace google::scp::core::common
