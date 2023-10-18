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

#include "job_client_utils.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/common/serialization/src/serialization.h"
#include "core/interface/async_context.h"
#include "core/utils/src/base64.h"
#include "google/protobuf/util/time_util.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

using google::cmrt::sdk::job_service::v1::Job;
using google::cmrt::sdk::job_service::v1::JobStatus;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::Item;
using google::cmrt::sdk::nosql_database_service::v1::ItemAttribute;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest;
using google::protobuf::Any;
using google::protobuf::util::TimeUtil;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::Serialization;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_DESERIALIZATION_FAILED;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_SERIALIZATION_FAILED;
using google::scp::core::utils::Base64Decode;
using google::scp::core::utils::Base64Encode;
using std::make_shared;
using std::map;
using std::none_of;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;

namespace {
constexpr char kJobsTablePartitionKeyName[] = "JobId";
constexpr char kServerJobIdColumnName[] = "ServerJobId";
constexpr char kJobBodyColumnName[] = "JobBody";
constexpr char kJobStatusColumnName[] = "JobStatus";
constexpr char kCreatedTimeColumnName[] = "CreatedTime";
constexpr char kUpdatedTimeColumnName[] = "UpdatedTime";
constexpr char kRetryCountColumnName[] = "RetryCount";
constexpr char kProcessingStartedTimeColumnName[] = "ProcessingStartedTime";
const vector<string> kJobsTableRequiredColumns = {
    kServerJobIdColumnName,
    kJobBodyColumnName,
    kJobStatusColumnName,
    kCreatedTimeColumnName,
    kUpdatedTimeColumnName,
    kRetryCountColumnName,
    kProcessingStartedTimeColumnName};
const google::protobuf::Timestamp kDefaultTimestampValue =
    TimeUtil::SecondsToTimestamp(0);

const map<JobStatus, set<JobStatus>> allowed_status_to_update = {
    {JobStatus::JOB_STATUS_CREATED,
     {JobStatus::JOB_STATUS_PROCESSING, JobStatus::JOB_STATUS_SUCCESS,
      JobStatus::JOB_STATUS_FAILURE}},
    {JobStatus::JOB_STATUS_PROCESSING,
     {JobStatus::JOB_STATUS_PROCESSING, JobStatus::JOB_STATUS_SUCCESS,
      JobStatus::JOB_STATUS_FAILURE}}};

ExecutionResult ValidateJobItem(const Item& item) noexcept {
  if (!item.has_key() || !item.key().has_partition_key() ||
      item.attributes_size() != kJobsTableRequiredColumns.size()) {
    return FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM);
  }

  for (const auto& column : kJobsTableRequiredColumns) {
    if (none_of(item.attributes().begin(), item.attributes().end(),
                [&column](const auto& attribute) {
                  return attribute.name() == column;
                })) {
      return FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM);
    }
  }

  return SuccessExecutionResult();
}

map<string, ItemAttribute> GetItemAttributes(const Item& item) noexcept {
  map<string, ItemAttribute> item_attributes_map;
  for (const auto& attribute : item.attributes()) {
    item_attributes_map.insert({attribute.name(), attribute});
  }
  return item_attributes_map;
}
}  // namespace

namespace google::scp::cpio::client_providers {
ItemAttribute JobClientUtils::MakeStringAttribute(
    const string& name, const string& value) noexcept {
  ItemAttribute attribute;
  attribute.set_name(name);
  attribute.set_value_string(value);
  return attribute;
}

ItemAttribute JobClientUtils::MakeIntAttribute(const string& name,
                                               const int32_t value) noexcept {
  ItemAttribute attribute;
  attribute.set_name(name);
  attribute.set_value_int(value);
  return attribute;
}

Job JobClientUtils::CreateJob(
    const string& job_id, const string& server_job_id, const Any& job_body,
    const JobStatus& job_status,
    const google::protobuf::Timestamp& created_time,
    const google::protobuf::Timestamp& updated_time,
    const google::protobuf::Timestamp& processing_started_time,
    const int retry_count) noexcept {
  Job job;
  job.set_job_id(job_id);
  job.set_server_job_id(server_job_id);
  job.set_job_status(job_status);
  *job.mutable_job_body() = job_body;
  *job.mutable_created_time() = created_time;
  *job.mutable_updated_time() = updated_time;
  job.set_retry_count(retry_count);
  *job.mutable_processing_started_time() = processing_started_time;
  return job;
}

ExecutionResultOr<string> JobClientUtils::ConvertAnyToBase64String(
    const Any& any) noexcept {
  string converted_string;
  bool result = any.SerializeToString(&converted_string);
  if (!result) {
    return FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_SERIALIZATION_FAILED);
  }
  string encoded_string;
  RETURN_IF_FAILURE(Base64Encode(converted_string, encoded_string));
  return encoded_string;
}

ExecutionResultOr<Any> JobClientUtils::ConvertBase64StringToAny(
    const string& str) noexcept {
  string decoded_string;
  RETURN_IF_FAILURE(Base64Decode(str, decoded_string));
  Any converted_any;
  bool result = converted_any.ParseFromString(decoded_string);
  if (!result) {
    return FailureExecutionResult(
        SC_JOB_CLIENT_PROVIDER_DESERIALIZATION_FAILED);
  }
  return converted_any;
}

ExecutionResultOr<Job> JobClientUtils::ConvertDatabaseItemToJob(
    const Item& item) noexcept {
  RETURN_IF_FAILURE(ValidateJobItem(item));

  const string& job_id = item.key().partition_key().value_string();
  const auto job_attributes_map = GetItemAttributes(item);

  const string& server_job_id =
      job_attributes_map.at(kServerJobIdColumnName).value_string();
  ASSIGN_OR_RETURN(
      auto job_body,
      ConvertBase64StringToAny(
          job_attributes_map.at(kJobBodyColumnName).value_string()));
  const JobStatus& job_status = static_cast<JobStatus>(
      job_attributes_map.at(kJobStatusColumnName).value_int());
  google::protobuf::Timestamp created_time;
  TimeUtil::FromString(
      job_attributes_map.at(kCreatedTimeColumnName).value_string(),
      &created_time);
  google::protobuf::Timestamp updated_time;
  TimeUtil::FromString(
      job_attributes_map.at(kUpdatedTimeColumnName).value_string(),
      &updated_time);
  const int retry_count =
      job_attributes_map.at(kRetryCountColumnName).value_int();
  google::protobuf::Timestamp processing_started_time;
  TimeUtil::FromString(
      job_attributes_map.at(kProcessingStartedTimeColumnName).value_string(),
      &processing_started_time);

  return CreateJob(job_id, server_job_id, job_body, job_status, created_time,
                   updated_time, processing_started_time, retry_count);
}

shared_ptr<UpsertDatabaseItemRequest> JobClientUtils::CreatePutJobRequest(
    const string& job_table_name, const Job& job,
    const string& job_body_as_string) noexcept {
  auto request =
      CreateUpsertJobRequest(job_table_name, job, job_body_as_string);
  *request->add_required_attributes() =
      MakeStringAttribute(kJobsTablePartitionKeyName, "");
  /* server_job_id is always unique for the job, so it is needed to be set as a
   * requried attribute in the request. This is to prevent a later PutJobRequest
   * with same job_id but different server_job_id to override exisiting job
   * entry.
   */
  *request->add_required_attributes() =
      MakeStringAttribute(kServerJobIdColumnName, "");
  return request;
}

shared_ptr<UpsertDatabaseItemRequest> JobClientUtils::CreateUpsertJobRequest(
    const string& job_table_name, const Job& job,
    const string& job_body_as_string) noexcept {
  auto request = make_shared<UpsertDatabaseItemRequest>();

  request->mutable_key()->set_table_name(job_table_name);

  *request->mutable_key()->mutable_partition_key() =
      MakeStringAttribute(kJobsTablePartitionKeyName, job.job_id());

  if (!job.server_job_id().empty()) {
    *request->add_new_attributes() =
        MakeStringAttribute(kServerJobIdColumnName, job.server_job_id());
  }
  if (!job_body_as_string.empty()) {
    *request->add_new_attributes() =
        MakeStringAttribute(kJobBodyColumnName, job_body_as_string);
  }
  if (job.job_status() != JobStatus::JOB_STATUS_UNKNOWN) {
    *request->add_new_attributes() =
        MakeIntAttribute(kJobStatusColumnName, job.job_status());
  }
  if (job.created_time() != kDefaultTimestampValue) {
    *request->add_new_attributes() = MakeStringAttribute(
        kCreatedTimeColumnName, TimeUtil::ToString(job.created_time()));
  }
  if (job.updated_time() != kDefaultTimestampValue) {
    *request->add_new_attributes() = MakeStringAttribute(
        kUpdatedTimeColumnName, TimeUtil::ToString(job.updated_time()));
  }
  *request->add_new_attributes() =
      MakeIntAttribute(kRetryCountColumnName, job.retry_count());
  if (job.processing_started_time() != kDefaultTimestampValue) {
    *request->add_new_attributes() =
        MakeStringAttribute(kProcessingStartedTimeColumnName,
                            TimeUtil::ToString(job.processing_started_time()));
  }
  return request;
}

shared_ptr<GetDatabaseItemRequest> JobClientUtils::CreateGetNextJobRequest(
    const string& job_table_name, const string& job_id,
    const string& server_job_id) noexcept {
  auto request = CreateGetJobByJobIdRequest(job_table_name, job_id);

  /* the server_job_id from the job message from the queue has to be the same as
   * the one in the job entry in the database. If not, the request will fail
   * with SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND.
   */
  *request->add_required_attributes() =
      MakeStringAttribute(kServerJobIdColumnName, server_job_id);

  return request;
}

shared_ptr<GetDatabaseItemRequest> JobClientUtils::CreateGetJobByJobIdRequest(
    const string& job_table_name, const string& job_id) noexcept {
  auto request = make_shared<GetDatabaseItemRequest>();

  request->mutable_key()->set_table_name(job_table_name);

  request->mutable_key()->mutable_partition_key()->set_name(
      kJobsTablePartitionKeyName);
  request->mutable_key()->mutable_partition_key()->set_value_string(job_id);

  return request;
}

ExecutionResult JobClientUtils::ValidateJobStatus(
    const JobStatus& current_status, const JobStatus& update_status) noexcept {
  const auto& current_status_set_iterator =
      allowed_status_to_update.find(current_status);
  if (current_status_set_iterator == allowed_status_to_update.end()) {
    return FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS);
  }
  const auto& current_status_set = current_status_set_iterator->second;
  return current_status_set.find(update_status) == current_status_set.end()
             ? FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS)
             : SuccessExecutionResult();
}

}  // namespace google::scp::cpio::client_providers
