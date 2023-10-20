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

#include <gmock/gmock.h>

#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"

namespace google::scp::cpio::client_providers::mock {

/*! @copydoc NoSQLDatabaseClientProviderInterface
 */
class MockNoSQLDatabaseClientProvider
    : public NoSQLDatabaseClientProviderInterface {
 public:
  MOCK_METHOD(core::ExecutionResult, Init, (), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, Run, (), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, Stop, (), (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, CreateTable,
      ((core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::CreateTableRequest,
          cmrt::sdk::nosql_database_service::v1::CreateTableResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, DeleteTable,
      ((core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::DeleteTableRequest,
          cmrt::sdk::nosql_database_service::v1::DeleteTableResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, GetDatabaseItem,
      ((core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, UpsertDatabaseItem,
      ((core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>&)),
      (noexcept, override));
};

}  // namespace google::scp::cpio::client_providers::mock
