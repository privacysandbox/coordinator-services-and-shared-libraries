// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "cc/pbs/consume_budget/src/gcp/consume_budget.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/pbs/consume_budget/src/budget_consumer.h"
#include "cc/pbs/consume_budget/src/gcp/error_codes.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/public/core/interface/errors.h"
#include "cc/public/core/interface/execution_result.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/mutations.h"

namespace privacy_sandbox::pbs {
namespace {

using ::privacy_sandbox::pbs_common::AsyncContext;
using ::privacy_sandbox::pbs_common::AsyncExecutorInterface;
using ::privacy_sandbox::pbs_common::AsyncPriority;
using ::privacy_sandbox::pbs_common::ConfigProviderInterface;
using ::privacy_sandbox::pbs_common::ExecutionResult;
using ::privacy_sandbox::pbs_common::ExecutionResultOr;
using ::privacy_sandbox::pbs_common::FailureExecutionResult;
using ::privacy_sandbox::pbs_common::GetErrorMessage;
using ::privacy_sandbox::pbs_common::kGcpProjectId;
using ::privacy_sandbox::pbs_common::kSpannerDatabase;
using ::privacy_sandbox::pbs_common::kSpannerEndpointOverride;
using ::privacy_sandbox::pbs_common::kSpannerInstance;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;
namespace spanner = ::google::cloud::spanner;

constexpr absl::string_view kComponentName = "BudgetConsumptionHelper";
}  // namespace

BudgetConsumptionHelper::BudgetConsumptionHelper(
    ConfigProviderInterface* config_provider,
    AsyncExecutorInterface* async_executor,
    AsyncExecutorInterface* io_async_executor,
    std::shared_ptr<spanner::Connection> spanner_connection)
    : config_provider_(config_provider),
      async_executor_(async_executor),
      io_async_executor_(io_async_executor),
      spanner_connection_(std::move(spanner_connection)) {}

ExecutionResultOr<std::shared_ptr<spanner::Connection>>
BudgetConsumptionHelper::MakeSpannerConnectionForProd(
    ConfigProviderInterface& config_provider) {
  std::string project;
  if (auto execution_result = config_provider.Get(kGcpProjectId, project);
      !execution_result.Successful()) {
    return execution_result;
  }

  std::string instance;
  if (auto execution_result = config_provider.Get(kSpannerInstance, instance);
      !execution_result.Successful()) {
    return execution_result;
  }

  std::string database;
  if (auto execution_result = config_provider.Get(kSpannerDatabase, database);
      !execution_result.Successful()) {
    return execution_result;
  }

  google::cloud::Options options;

  std::string endpoint_override;
  if (auto execution_result =
          config_provider.Get(kSpannerEndpointOverride, endpoint_override);
      execution_result.Successful()) {
    options.set<google::cloud::EndpointOption>(endpoint_override);
  }

  return spanner::MakeConnection(spanner::Database(project, instance, database),
                                 options);
}

ExecutionResult BudgetConsumptionHelper::Init() noexcept {
  if (!spanner_connection_) {
    return FailureExecutionResult(SC_CONSUME_BUDGET_INITIALIZATION_ERROR);
  }
  if (auto execution_result =
          config_provider_->Get(kBudgetKeyTableName, table_name_);
      execution_result != SuccessExecutionResult()) {
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult BudgetConsumptionHelper::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult BudgetConsumptionHelper::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult BudgetConsumptionHelper::ConsumeBudgets(
    AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
        consume_budgets_context) {
  // TODO: Check that request is not empty.
  // Return invalid argument
  if (auto schedule_result = io_async_executor_->Schedule(
          [this, consume_budgets_context]() {
            ConsumeBudgetsSyncAndFinishContext(consume_budgets_context);
          },
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    // Returns the execution result to the caller without calling FinishContext,
    // since the async task is not scheduled successfully
    return schedule_result;
  }
  return SuccessExecutionResult();
}

void BudgetConsumptionHelper::ConsumeBudgetsSyncAndFinishContext(
    AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
        consume_budgets_context) {
  consume_budgets_context.result =
      ConsumeBudgetsSyncWithBudgetConsumer(consume_budgets_context);

  if (!async_executor_->Schedule(
          [consume_budgets_context]() mutable {
            consume_budgets_context.Finish();
          },
          AsyncPriority::Normal)) {
    consume_budgets_context.Finish();
  }
}

ExecutionResult BudgetConsumptionHelper::ConsumeBudgetsSyncWithBudgetConsumer(
    const AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>&
        consume_budgets_context) {
  spanner::Client client(spanner_connection_);
  ExecutionResult captured_execution_result = SuccessExecutionResult();

  auto commit_result =
      client.Commit([&](spanner::Transaction txn)
                        -> google::cloud::StatusOr<spanner::Mutations> {
        BudgetConsumer& budget_consumer =
            *consume_budgets_context.request->budget_consumer;
        spanner::KeySet spanner_key_set = budget_consumer.GetSpannerKeySet();

        auto columns = budget_consumer.GetReadColumns();
        if (!columns.Successful()) {
          captured_execution_result =
              FailureExecutionResult(SC_CONSUME_BUDGET_INITIALIZATION_ERROR);
          return google::cloud::Status(
              google::cloud::StatusCode::kInvalidArgument,
              "Cannot fetch the columns to read");
        }

        spanner::RowStream row_stream =
            client.Read(txn, table_name_, std::move(spanner_key_set), *columns);
        SpannerMutationsResult spanner_mutations_result =
            budget_consumer.ConsumeBudget(row_stream, table_name_);

        consume_budgets_context.response->budget_exhausted_indices =
            spanner_mutations_result.budget_exhausted_indices;
        captured_execution_result = spanner_mutations_result.execution_result;
        if (spanner_mutations_result.status.ok()) {
          return spanner_mutations_result.mutations;
        }
        return spanner_mutations_result.status;
      });

  if (!commit_result) {
    // If the error status is coming from PBS's application logics, the
    // captured_execution_result should contain failure result. Otherwise, the
    // error status is considered to be coming from Spanner. In this case, the
    // spanner error codes will be transformed to a failed ExecutionResult with
    // INTERNAL SERVER ERROR.
    auto final_execution_result =
        !captured_execution_result.Successful()
            ? captured_execution_result
            : FailureExecutionResult(SC_CONSUME_BUDGET_FAIL_TO_COMMIT);
    if (captured_execution_result.status_code == SC_CONSUME_BUDGET_EXHAUSTED) {
      SCP_WARNING_CONTEXT(
          kComponentName, consume_budgets_context,
          absl::StrFormat("ConsumeBudgets failed. Error code %d, message: %s, "
                          "final_execution_result: %s",
                          commit_result.status().code(),
                          commit_result.status().message(),
                          GetErrorMessage(final_execution_result.status_code)));
    } else {
      SCP_ERROR_CONTEXT(
          kComponentName, consume_budgets_context, final_execution_result,
          absl::StrFormat("ConsumeBudgets failed. Error code %d, message: %s",
                          commit_result.status().code(),
                          commit_result.status().message()));
    }
    return final_execution_result;
  }
  return SuccessExecutionResult();
}
}  // namespace privacy_sandbox::pbs
