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

#pragma once

#include <list>
#include <mutex>
#include <tuple>
#include <utility>

#include "absl/container/flat_hash_map.h"

namespace google::scp::core::common {
/**
 * @brief Least Recently Used (LRU) cache
 *
 * @tparam TKey
 * @tparam TVal
 */
template <typename TKey, typename TVal>
class LruCache {
 public:
  explicit LruCache(size_t capacity) : capacity_(capacity) {}

  void Set(const TKey& key, const TVal& value) {
    std::lock_guard lock(data_mutex_);

    InternalSet(key, value);
  }

  TVal& Get(const TKey& key) {
    std::lock_guard lock(data_mutex_);

    auto existing_element = data_[key];
    auto value = std::get<1>(existing_element);
    // Set to update the freshness of the element
    InternalSet(key, value);
    return std::get<1>(data_[key]);
  }

  size_t Size() {
    std::lock_guard lock(data_mutex_);
    return data_.size();
  }

  size_t Capacity() { return capacity_; }

  bool Contains(const TKey& key) {
    std::lock_guard lock(data_mutex_);
    return data_.contains(key);
  }

  void Clear() {
    std::lock_guard lock(data_mutex_);
    data_.clear();
    freshness_list_.clear();
  }

  absl::flat_hash_map<TKey, TVal> GetAll() {
    std::lock_guard lock(data_mutex_);

    absl::flat_hash_map<TKey, TVal> result;

    for (auto& kv : data_) {
      result[kv.first] = std::get<1>(kv.second);
    }

    return result;
  }

 private:
  const size_t capacity_;
  absl::flat_hash_map<TKey,
                      std::tuple<typename std::list<TKey>::iterator, TVal>>
      data_;
  /**
   * @brief List to keep the order of access. Fresh items will be at the
   * beginning, and stale items at the end. The last item is what would be
   * removed if the cache reaches capacity.
   *
   */
  std::list<TKey> freshness_list_;
  std::mutex data_mutex_;

  void InternalSet(const TKey& key, const TVal& value) {
    if (!data_.contains(key) && data_.size() == capacity_) {
      // If the element is not in the cache and we reached capacity, let's
      // remove the LRU element. We look at the list of freshness to determine
      // which element is the LRU, and use the key from it to remove the data
      // from the data map.
      auto end_iterator = freshness_list_.end();
      auto element_to_evict_iterator = --end_iterator;
      data_.erase(*element_to_evict_iterator);
      freshness_list_.pop_back();
    }

    if (data_.contains(key)) {
      // If this is an existing element, let's remove it from the freshness
      // list.
      auto existing_element = data_[key];
      freshness_list_.erase(std::get<0>(existing_element));
    }

    freshness_list_.push_front(key);
    auto front_iterator = freshness_list_.begin();
    data_[key] = std::move(std::make_tuple(front_iterator, value));
  }
};
}  // namespace google::scp::core::common
