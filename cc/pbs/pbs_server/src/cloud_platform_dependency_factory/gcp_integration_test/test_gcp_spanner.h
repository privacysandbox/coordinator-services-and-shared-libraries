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
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/src/gcp/gcp_spanner.h"

namespace google::scp::pbs {
/*! @copydoc GcpSpanner
 */
class TestGcpSpanner : public core::nosql_database_provider::GcpSpanner {
 public:
  TestGcpSpanner(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      std::unique_ptr<std::unordered_map<
          std::string, std::pair<std::string, std::optional<std::string>>>>
          table_name_to_keys)
      : GcpSpanner(async_executor, io_async_executor, config_provider,
                   std::move(table_name_to_keys), core::AsyncPriority::Normal,
                   core::AsyncPriority::Normal) {}

 protected:
  void CreateSpanner(const std::string& project, const std::string& instance,
                     const std::string& database) noexcept override;
};
}  // namespace google::scp::pbs
