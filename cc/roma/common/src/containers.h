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

#include <functional>
#include <list>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"

namespace google::scp::roma::common {
/**
 * @brief This is a map implementation which allows iterating over the map
 * using the order of insertion. The order of insertion can be retrieved by
 * calling Keys(), which returns the keys in the order in which they were
 * inserted.
 * Set, Get, Contains, Delete and Size are O(1). Keys is O(N)
 *
 * @tparam TKey
 * @tparam TVal
 */
template <typename TKey, typename TVal>
class Map {
 public:
  /**
   * @brief Insert a key value pair into the map
   *
   * @param key
   * @param val
   */
  void Set(TKey&& key, TVal& val) {
    std::tuple<typename std::list<TKey>::iterator, TVal> val_tuple;

    if (!Contains(key)) {
      order_list_.push_back(key);
      auto end_iterator = order_list_.end();
      auto inserted_element_iterator = --end_iterator;
      val_tuple = std::make_tuple(inserted_element_iterator, val);
    } else {
      auto existing_tuple = map_[key];
      // We need to keep the iterator to the key so that the insertion order can
      // be preserved.
      val_tuple = std::make_tuple(std::get<0>(existing_tuple), val);
    }

    map_[key] = val_tuple;
  }

  /**
   * @brief Insert a key value pair into the map
   *
   * @param key
   * @param val
   */
  void Set(const TKey& key, const TVal& val) {
    std::tuple<typename std::list<TKey>::iterator, TVal> val_tuple;

    if (!Contains(key)) {
      order_list_.push_back(key);
      auto end_iterator = order_list_.end();
      auto inserted_element_iterator = --end_iterator;
      val_tuple = std::make_tuple(inserted_element_iterator, val);
    } else {
      auto existing_tuple = map_[key];
      // We need to keep the iterator to the key so that the insertion order can
      // be preserved.
      val_tuple = std::make_tuple(std::get<0>(existing_tuple), val);
    }

    map_[key] = val_tuple;
  }

  /**
   * @brief Check whether the map container a given element by key
   *
   * @param key
   * @return true
   * @return false
   */
  bool Contains(TKey&& key) { return map_.contains(key); }

  /**
   * @brief Check whether the map container a given element by key
   *
   * @param key
   * @return true
   * @return false
   */
  bool Contains(const TKey& key) { return map_.contains(key); }

  /**
   * @brief Remove an element from the map by key
   *
   * @param key
   */
  void Delete(TKey&& key) {
    auto val = map_[key];
    auto it = std::get<0>(val);
    order_list_.remove(*it);
    map_.erase(key);
  }

  /**
   * @brief Remove an element from the map by key
   *
   * @param key
   */
  void Delete(const TKey& key) {
    auto val = map_[key];
    auto it = std::get<0>(val);
    order_list_.remove(*it);
    map_.erase(key);
  }

  /**
   * @brief Get an element from the map by key
   *
   * @param key
   * @return TVal&
   */
  TVal& Get(TKey&& key) { return std::get<1>(map_[key]); }

  /**
   * @brief Get an element from the map by key
   *
   * @param key
   * @return TVal&
   */
  TVal& Get(const TKey& key) { return std::get<1>(map_[key]); }

  /**
   * @brief Get the keys in the order in which they were inserted
   *
   * @return std::vector<TKey>
   */
  std::vector<TKey> Keys() {
    return std::vector<TKey>(order_list_.begin(), order_list_.end());
  }

  /**
   * @brief Clear the data in the map
   *
   */
  void Clear() {
    map_.clear();
    order_list_.clear();
  }

  /**
   * @brief Get the size of the map
   *
   * @return size_t
   */
  size_t Size() { return map_.size(); }

 private:
  absl::flat_hash_map<TKey,
                      std::tuple<typename std::list<TKey>::iterator, TVal>>
      map_;
  std::list<TKey> order_list_;
};
}  // namespace google::scp::roma::common
