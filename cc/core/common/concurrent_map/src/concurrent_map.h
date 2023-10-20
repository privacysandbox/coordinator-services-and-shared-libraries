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

#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "oneapi/tbb/concurrent_hash_map.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::core::common {
/**
 * @brief ConcurrentMap provides multi producers and multi consumers map
 * support to be used generically.
 */
template <class TKey, class TValue,
          typename TCompare = oneapi::tbb::tbb_hash_compare<TKey>>
class ConcurrentMap {
  /// The current library relies on OneApi::tbb library.
  typedef oneapi::tbb::concurrent_hash_map<TKey, TValue, TCompare>
      ConcurrentMapImpl;

 public:
  // TODO: We might need to look into keeping the size constant.

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
  ExecutionResult Insert(std::pair<TKey, TValue> key_value, TValue& out_value) {
    std::shared_lock lock(concurrent_map_mutex_);

    typename ConcurrentMapImpl::accessor map_accessor;
    ExecutionResult execution_result = SuccessExecutionResult();

    if (!concurrent_map_.insert(map_accessor, key_value)) {
      execution_result = FailureExecutionResult(
          errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS);
    }

    out_value = map_accessor->second;
    map_accessor.release();
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
  ExecutionResult Find(const TKey& key, TValue& out_value) {
    std::shared_lock lock(concurrent_map_mutex_);

    typename ConcurrentMapImpl::accessor map_accessor;
    ExecutionResult execution_result = SuccessExecutionResult();

    if (!concurrent_map_.find(map_accessor, key)) {
      execution_result = FailureExecutionResult(
          errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST);
    } else {
      out_value = map_accessor->second;
    }

    map_accessor.release();
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
  ExecutionResult Erase(const TKey& key) {
    std::shared_lock lock(concurrent_map_mutex_);
    ExecutionResult execution_result = SuccessExecutionResult();

    if (!concurrent_map_.erase(key)) {
      execution_result = FailureExecutionResult(
          errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST);
    }

    return execution_result;
  }

  /**
   * @brief Gets all the keys in the current concurrent map. This is a very
   * expensive operation due to locking.
   *
   * @param keys A vector of the keys to be filled in once looked up.
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult Keys(std::vector<TKey>& keys) {
    std::unique_lock lock(concurrent_map_mutex_);

    keys.clear();
    for (auto it = concurrent_map_.begin(); it != concurrent_map_.end(); ++it) {
      keys.push_back(it->first);
    }

    return SuccessExecutionResult();
  }

  /**
   * @brief Returns the current size of the concurrent map in a thread-safe way.
   *
   * @return size_t
   */
  size_t Size() const { return concurrent_map_.size(); }

 private:
  /// Concurrent map implementation.
  ConcurrentMapImpl concurrent_map_;

  /// Mutex to prevent write operations during Keys calls.
  mutable std::shared_timed_mutex concurrent_map_mutex_;
};
}  // namespace google::scp::core::common
