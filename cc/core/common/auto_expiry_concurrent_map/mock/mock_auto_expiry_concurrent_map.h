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

#include <memory>
#include <utility>

#include "core/common/auto_expiry_concurrent_map/src/auto_expiry_concurrent_map.h"

namespace google::scp::core::common::auto_expiry_concurrent_map::mock {

template <class TKey, class TValue,
          typename TCompare = oneapi::tbb::tbb_hash_compare<TKey>>
class MockAutoExpiryConcurrentMap
    : public common::AutoExpiryConcurrentMap<TKey, TValue, TCompare> {
 public:
  MockAutoExpiryConcurrentMap(
      size_t map_entry_lifetime_seconds, bool extend_entry_lifetime_on_access,
      bool block_entry_while_eviction,
      std::function<void(TKey&, TValue&, std::function<void(bool)>)>
          on_before_element_deletion_callback,
      const std::shared_ptr<AsyncExecutorInterface>& async_executor)
      : common::AutoExpiryConcurrentMap<TKey, TValue, TCompare>(
            map_entry_lifetime_seconds, extend_entry_lifetime_on_access,
            block_entry_while_eviction, on_before_element_deletion_callback,
            async_executor) {}

  auto& GetUnderlyingConcurrentMap() {
    return AutoExpiryConcurrentMap<TKey, TValue, TCompare>::concurrent_map_;
  }

  void RunGarbageCollector() {
    AutoExpiryConcurrentMap<TKey, TValue, TCompare>::RunGarbageCollector();
  }

  bool IsEvictable(TKey& key) {
    std::shared_ptr<typename AutoExpiryConcurrentMap<
        TKey, TValue, TCompare>::AutoExpiryConcurrentMapEntry>
        entry;
    GetUnderlyingConcurrentMap().Find(key, entry);
    return entry->is_evictable;
  }

  void MarkAsBeingDeleted(TKey& key) {
    std::shared_ptr<typename AutoExpiryConcurrentMap<
        TKey, TValue, TCompare>::AutoExpiryConcurrentMapEntry>
        entry;
    GetUnderlyingConcurrentMap().Find(key, entry);
    entry->being_evicted = true;
  }

  void MarkAsNotBeingDeleted(TKey& key) {
    std::shared_ptr<typename AutoExpiryConcurrentMap<
        TKey, TValue, TCompare>::AutoExpiryConcurrentMapEntry>
        entry;
    GetUnderlyingConcurrentMap().Find(key, entry);
    entry->being_evicted = false;
  }

  void OnRemoveEntryFromCacheLogged(
      std::pair<TKey,
                std::shared_ptr<typename AutoExpiryConcurrentMap<
                    TKey, TValue, TCompare>::AutoExpiryConcurrentMapEntry>>&
          key_value_pair,
      bool can_delete) noexcept {
    AutoExpiryConcurrentMap<
        TKey, TValue, TCompare>::OnRemoveEntryFromCacheLogged(key_value_pair,
                                                              can_delete);
  }

  virtual ExecutionResult Insert(std::pair<TKey, TValue> key_value,
                                 TValue& out_value) noexcept {
    if (insert_mock) {
      return insert_mock();
    }

    return AutoExpiryConcurrentMap<TKey, TValue, TCompare>::Insert(key_value,
                                                                   out_value);
  }

  std::function<ExecutionResult()> insert_mock;
};
}  // namespace google::scp::core::common::auto_expiry_concurrent_map::mock
