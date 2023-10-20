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

#include "public/cpio/adapters/job_client/src/job_client.h"

#include <gtest/gtest.h>

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/job_client/mock/mock_job_client_with_overrides.h"
#include "public/cpio/interface/job_client/job_client_interface.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"

using google::cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest;
using google::cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse;
using google::cmrt::sdk::job_service::v1::GetJobByIdRequest;
using google::cmrt::sdk::job_service::v1::GetJobByIdResponse;
using google::cmrt::sdk::job_service::v1::GetNextJobRequest;
using google::cmrt::sdk::job_service::v1::GetNextJobResponse;
using google::cmrt::sdk::job_service::v1::PutJobRequest;
using google::cmrt::sdk::job_service::v1::PutJobResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobBodyRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobBodyResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobStatusRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobStatusResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse;
using google::scp::core::AsyncContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::cpio::mock::MockJobClientWithOverrides;
using testing::Return;

namespace google::scp::cpio::test {

class JobClientTest : public ::testing::Test {
 protected:
  JobClientTest() { assert(client_.Init().Successful()); }

  MockJobClientWithOverrides client_;
};

TEST_F(JobClientTest, PutJobSuccess) {
  AsyncContext<PutJobRequest, PutJobResponse> context;
  EXPECT_CALL(client_.GetJobClientProvider(), PutJob)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.PutJob(context), IsSuccessful());
}

TEST_F(JobClientTest, GetNextJobSuccess) {
  AsyncContext<GetNextJobRequest, GetNextJobResponse> context;
  EXPECT_CALL(client_.GetJobClientProvider(), GetNextJob)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.GetNextJob(context), IsSuccessful());
}

TEST_F(JobClientTest, GetJobByIdSuccess) {
  AsyncContext<GetJobByIdRequest, GetJobByIdResponse> context;
  EXPECT_CALL(client_.GetJobClientProvider(), GetJobById)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.GetJobById(context), IsSuccessful());
}

TEST_F(JobClientTest, UpdateJobBodySuccess) {
  AsyncContext<UpdateJobBodyRequest, UpdateJobBodyResponse> context;
  EXPECT_CALL(client_.GetJobClientProvider(), UpdateJobBody)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.UpdateJobBody(context), IsSuccessful());
}

TEST_F(JobClientTest, UpdateJobStatusSuccess) {
  AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse> context;
  EXPECT_CALL(client_.GetJobClientProvider(), UpdateJobStatus)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.UpdateJobStatus(context), IsSuccessful());
}

TEST_F(JobClientTest, UpdateJobVisibilityTimeout) {
  AsyncContext<UpdateJobVisibilityTimeoutRequest,
               UpdateJobVisibilityTimeoutResponse>
      context;
  EXPECT_CALL(client_.GetJobClientProvider(), UpdateJobVisibilityTimeout)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.UpdateJobVisibilityTimeout(context), IsSuccessful());
}

TEST_F(JobClientTest, DeleteOrphanedJobMessage) {
  AsyncContext<DeleteOrphanedJobMessageRequest,
               DeleteOrphanedJobMessageResponse>
      context;
  EXPECT_CALL(client_.GetJobClientProvider(), DeleteOrphanedJobMessage)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_.DeleteOrphanedJobMessage(context), IsSuccessful());
}

}  // namespace google::scp::cpio::test
