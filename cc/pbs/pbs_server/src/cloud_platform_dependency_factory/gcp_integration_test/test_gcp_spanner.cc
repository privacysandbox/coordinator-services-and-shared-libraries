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

#include "test_gcp_spanner.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/interface/async_context.h"
#include "core/interface/configuration_keys.h"
#include "core/nosql_database_provider/src/common/error_codes.h"
#include "core/nosql_database_provider/src/common/nosql_database_provider_utils.h"

#include "test_configuration_keys.h"

using google::cloud::EndpointOption;
using google::cloud::Options;
using google::cloud::spanner::Client;
using google::cloud::spanner::Database;
using std::make_shared;
using std::string;

namespace google::scp::pbs {

void TestGcpSpanner::CreateSpanner(const string& project,
                                   const string& instance,
                                   const string& database) noexcept {
  string endpoint_override;
  config_provider_->Get(kSpannerEndpointOverride, endpoint_override);

  auto options = Options{}.set<EndpointOption>(endpoint_override);

  spanner_client_shared_ = make_shared<Client>(
      MakeConnection(Database(project, instance, database), options));
}
}  // namespace google::scp::pbs
