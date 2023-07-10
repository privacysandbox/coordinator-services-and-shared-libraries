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
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/src/common/error_codes.h"

namespace google::scp::core::nosql_database_provider::mock {
class MockNoSQLDatabaseProviderNoOverrides
    : public NoSQLDatabaseProviderInterface {
 public:
  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult GetDatabaseItem(
      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
          get_database_item_context) noexcept override {
    return get_database_item_mock(get_database_item_context);
  }

  ExecutionResult UpsertDatabaseItem(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
          upsert_database_item_context) noexcept override {
    return upsert_database_item_mock(upsert_database_item_context);
  }

  std::function<ExecutionResult(
      AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&)>
      get_database_item_mock;

  std::function<ExecutionResult(
      AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&)>
      upsert_database_item_mock;
};
}  // namespace google::scp::core::nosql_database_provider::mock
