// Copyright 2022 Google LLC
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

#include "cpio/client_providers/job_client_provider/src/gcp/gcp_job_client_provider.h"

#include <gtest/gtest.h>

#include "cc/cpio/client_providers/job_client_provider/src/error_codes.h"
#include "cpio/client_providers/nosql_database_client_provider/mock/mock_nosql_database_client_provider.h"
#include "cpio/client_providers/nosql_database_client_provider/src/common/error_codes.h"
#include "cpio/client_providers/queue_client_provider/mock/mock_queue_client_provider.h"
#include "public/core/test/interface/execution_result_matchers.h"
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::
    SC_JOB_CLIENT_PROVIDER_DUPLICATE_JOB_ENTRY_CREATION;
using google::scp::core::errors::
    SC_JOB_CLIENT_PROVIDER_JOB_ENTRY_CREATION_FAILED;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::mock::
    MockNoSQLDatabaseClientProvider;
using google::scp::cpio::client_providers::mock::MockQueueClientProvider;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;
using testing::NiceMock;

namespace {
constexpr char kJobsTableName[] = "Jobs";
}

namespace google::scp::cpio::client_providers::job_client::test {

class GcpJobClientProviderTest : public ::testing::Test {
 protected:
  GcpJobClientProviderTest() {
    job_client_options_ = make_shared<JobClientOptions>();
    job_client_options_->job_table_name = kJobsTableName;
    queue_client_provider_ = make_shared<NiceMock<MockQueueClientProvider>>();
    nosql_database_client_provider_ =
        make_shared<NiceMock<MockNoSQLDatabaseClientProvider>>();

    gcp_job_client_provider_ = make_unique<GcpJobClientProvider>(
        job_client_options_, queue_client_provider_,
        nosql_database_client_provider_);
  }

  void TearDown() override { EXPECT_SUCCESS(gcp_job_client_provider_->Stop()); }

  shared_ptr<JobClientOptions> job_client_options_;
  shared_ptr<MockQueueClientProvider> queue_client_provider_;
  shared_ptr<MockNoSQLDatabaseClientProvider> nosql_database_client_provider_;
  unique_ptr<GcpJobClientProvider> gcp_job_client_provider_;
};

TEST_F(GcpJobClientProviderTest,
       ConvertDatabaseErrorForPutJobWithConditionFailure) {
  auto status_code = SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND;
  EXPECT_THAT(
      gcp_job_client_provider_->ConvertDatabaseErrorForPutJob(status_code),
      ResultIs(FailureExecutionResult(
          SC_JOB_CLIENT_PROVIDER_DUPLICATE_JOB_ENTRY_CREATION)));
}

TEST_F(GcpJobClientProviderTest,
       ConvertDatabaseErrorForPutJobWithOtherFailure) {
  auto status_code = SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE;
  EXPECT_THAT(
      gcp_job_client_provider_->ConvertDatabaseErrorForPutJob(status_code),
      ResultIs(FailureExecutionResult(
          SC_JOB_CLIENT_PROVIDER_JOB_ENTRY_CREATION_FAILED)));
}

}  // namespace google::scp::cpio::client_providers::job_client::test
