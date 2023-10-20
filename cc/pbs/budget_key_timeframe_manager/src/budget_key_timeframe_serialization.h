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
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>

#include "core/common/serialization/src/serialization.h"
#include "pbs/budget_key_timeframe_manager/src/proto/budget_key_timeframe_manager.pb.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::pbs::budget_key_timeframe_manager {
static constexpr TimeBucket kHoursPerDay = 24;
static constexpr core::Version kCurrentVersion = {.major = 1, .minor = 0};

class Serialization {
 public:
  /**
   * @brief Serializes budget key time frame log.
   *
   * @param time_group The time group associated with the timeframe.
   * @param budget_key_timeframe The budget key timeframe to serialize to
   * log.
   * @param budget_key_timeframe_log_bytes_buffer The byte buffer to write
   * the data to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeBudgetKeyTimeframeLog(
      const TimeGroup& time_group,
      const std::shared_ptr<BudgetKeyTimeframe>& budget_key_timeframe,
      core::BytesBuffer& budget_key_timeframe_log_bytes_buffer) {
    core::BytesBuffer budget_key_timeframe_log_1_0_bytes_buffer;
    auto execution_result = SerializeBudgetKeyTimeframeLog_1_0(
        budget_key_timeframe, budget_key_timeframe_log_1_0_bytes_buffer);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    proto::BudgetKeyTimeframeManagerLog_1_0
        budget_key_timeframe_manager_log_1_0;
    budget_key_timeframe_manager_log_1_0.set_operation_type(
        proto::OperationType::UPDATE_TIMEFRAME_RECORD);
    budget_key_timeframe_manager_log_1_0.set_time_group(time_group);

    budget_key_timeframe_manager_log_1_0.set_log_body(
        budget_key_timeframe_log_1_0_bytes_buffer.bytes->data(),
        budget_key_timeframe_log_1_0_bytes_buffer.length);

    core::BytesBuffer budget_key_timeframe_manager_log_1_0_bytes_buffer;
    execution_result = SerializeBudgetKeyTimeframeManagerLog_1_0(
        budget_key_timeframe_manager_log_1_0,
        budget_key_timeframe_manager_log_1_0_bytes_buffer);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    proto::BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
    budget_key_timeframe_manager_log.mutable_version()->set_major(
        kCurrentVersion.major);
    budget_key_timeframe_manager_log.mutable_version()->set_minor(
        kCurrentVersion.minor);

    budget_key_timeframe_manager_log.set_log_body(
        budget_key_timeframe_manager_log_1_0_bytes_buffer.bytes->data(),
        budget_key_timeframe_manager_log_1_0_bytes_buffer.length);

    return SerializeBudgetKeyTimeframeManagerLog(
        budget_key_timeframe_manager_log,
        budget_key_timeframe_log_bytes_buffer);
  }

  /**
   * @brief Serializes provided budget key time frames to a batch budget key
   * time frame log.
   *
   * NOTE: All of the provided budget key time frames must belong to
   * same timegroup.
   *
   * @param time_group The time group associated with the timeframes provided.
   * @param budget_key_timeframes The budget key timeframes to serialize to
   * log.
   * @param batch_budget_key_timeframe_log_bytes_buffer The byte buffer to write
   * the data to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeBatchBudgetKeyTimeframeLog(
      const TimeGroup& time_group,
      const std::vector<std::shared_ptr<BudgetKeyTimeframe>>&
          budget_key_timeframes,
      core::BytesBuffer& batch_budget_key_timeframe_log_bytes_buffer) {
    core::BytesBuffer batch_budget_key_timeframe_log_1_0_bytes_buffer;
    auto execution_result = SerializeBatchBudgetKeyTimeframeLog_1_0(
        budget_key_timeframes, batch_budget_key_timeframe_log_1_0_bytes_buffer);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    proto::BudgetKeyTimeframeManagerLog_1_0
        budget_key_timeframe_manager_log_1_0;
    budget_key_timeframe_manager_log_1_0.set_operation_type(
        proto::OperationType::BATCH_UPDATE_TIMEFRAME_RECORDS_OF_TIMEGROUP);
    budget_key_timeframe_manager_log_1_0.set_time_group(time_group);

    budget_key_timeframe_manager_log_1_0.set_log_body(
        batch_budget_key_timeframe_log_1_0_bytes_buffer.bytes->data(),
        batch_budget_key_timeframe_log_1_0_bytes_buffer.length);

    core::BytesBuffer budget_key_timeframe_manager_log_1_0_bytes_buffer;
    execution_result = SerializeBudgetKeyTimeframeManagerLog_1_0(
        budget_key_timeframe_manager_log_1_0,
        budget_key_timeframe_manager_log_1_0_bytes_buffer);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    proto::BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
    budget_key_timeframe_manager_log.mutable_version()->set_major(
        kCurrentVersion.major);
    budget_key_timeframe_manager_log.mutable_version()->set_minor(
        kCurrentVersion.minor);

    budget_key_timeframe_manager_log.set_log_body(
        budget_key_timeframe_manager_log_1_0_bytes_buffer.bytes->data(),
        budget_key_timeframe_manager_log_1_0_bytes_buffer.length);

    return SerializeBudgetKeyTimeframeManagerLog(
        budget_key_timeframe_manager_log,
        batch_budget_key_timeframe_log_bytes_buffer);
  }

  /**
   * @brief Serializes budget key time frame group log.
   *
   * @param budget_key_timeframe_group The budget key timeframe group to
   * serialize to log.
   * @param budget_key_timeframe_group_log_bytes_buffer The byte buffer to write
   * the data to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeBudgetKeyTimeframeGroupLog(
      const std::shared_ptr<BudgetKeyTimeframeGroup>&
          budget_key_timeframe_group,
      core::BytesBuffer& budget_key_timeframe_group_log_bytes_buffer) {
    // Serialize all the log body
    core::BytesBuffer budget_key_timeframe_group_log_1_0_buffer;
    auto execution_result =
        Serialization::SerializeBudgetKeyTimeframeGroupLog_1_0(
            budget_key_timeframe_group,
            budget_key_timeframe_group_log_1_0_buffer);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    proto::BudgetKeyTimeframeManagerLog_1_0
        budget_key_timeframe_manager_log_1_0;
    budget_key_timeframe_manager_log_1_0.set_operation_type(
        proto::OperationType::INSERT_TIMEGROUP_INTO_CACHE);
    budget_key_timeframe_manager_log_1_0.set_time_group(
        budget_key_timeframe_group->time_group);

    budget_key_timeframe_manager_log_1_0.set_log_body(
        budget_key_timeframe_group_log_1_0_buffer.bytes->data(),
        budget_key_timeframe_group_log_1_0_buffer.length);

    core::BytesBuffer budget_key_timeframe_manager_log_1_0_bytes_buffer;
    execution_result = SerializeBudgetKeyTimeframeManagerLog_1_0(
        budget_key_timeframe_manager_log_1_0,
        budget_key_timeframe_manager_log_1_0_bytes_buffer);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    proto::BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
    budget_key_timeframe_manager_log.mutable_version()->set_major(
        kCurrentVersion.major);
    budget_key_timeframe_manager_log.mutable_version()->set_minor(
        kCurrentVersion.minor);

    budget_key_timeframe_manager_log.set_log_body(
        budget_key_timeframe_manager_log_1_0_bytes_buffer.bytes->data(),
        budget_key_timeframe_manager_log_1_0_bytes_buffer.length);

    return SerializeBudgetKeyTimeframeManagerLog(
        budget_key_timeframe_manager_log,
        budget_key_timeframe_group_log_bytes_buffer);
  }

  /**
   * @brief Serializes budget key time frame group removal log.
   *
   * @param budget_key_timeframe_group The budget key timeframe group to
   * serialize to log.
   * @param budget_key_timeframe_manager_group_removal_log_bytes_buffer The byte
   * buffer to write the data to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeBudgetKeyTimeframeGroupRemoval(
      const std::shared_ptr<BudgetKeyTimeframeGroup>&
          budget_key_timeframe_group,
      core::BytesBuffer&
          budget_key_timeframe_manager_group_removal_log_bytes_buffer) {
    proto::BudgetKeyTimeframeManagerLog_1_0
        budget_key_timeframe_manager_log_1_0;
    budget_key_timeframe_manager_log_1_0.set_operation_type(
        proto::OperationType::REMOVE_TIMEGROUP_FROM_CACHE);
    budget_key_timeframe_manager_log_1_0.set_time_group(
        budget_key_timeframe_group->time_group);

    core::BytesBuffer budget_key_timeframe_manager_log_1_0_bytes_buffer;
    auto execution_result = SerializeBudgetKeyTimeframeManagerLog_1_0(
        budget_key_timeframe_manager_log_1_0,
        budget_key_timeframe_manager_log_1_0_bytes_buffer);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    proto::BudgetKeyTimeframeManagerLog budget_key_timeframe_manager_log;
    budget_key_timeframe_manager_log.mutable_version()->set_major(
        kCurrentVersion.major);
    budget_key_timeframe_manager_log.mutable_version()->set_minor(
        kCurrentVersion.minor);

    budget_key_timeframe_manager_log.set_log_body(
        budget_key_timeframe_manager_log_1_0_bytes_buffer.bytes->data(),
        budget_key_timeframe_manager_log_1_0_bytes_buffer.length);

    return SerializeBudgetKeyTimeframeManagerLog(
        budget_key_timeframe_manager_log,
        budget_key_timeframe_manager_group_removal_log_bytes_buffer);
  }

  /**
   * @brief Serializes budget key time frame manager removal log.
   *
   * @param budget_key_timeframe_manager_log The log object to serialize.
   * @param budget_key_timeframe_log_bytes_buffer The byte
   * buffer to write the data to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeBudgetKeyTimeframeManagerLog(
      const proto::BudgetKeyTimeframeManagerLog&
          budget_key_timeframe_manager_log,
      core::BytesBuffer& budget_key_timeframe_log_bytes_buffer) {
    // For budget key time frame serialization is a considered as an update
    // operation to the entries in the cache.
    size_t offset = 0;
    size_t bytes_serialized = 0;
    budget_key_timeframe_log_bytes_buffer =
        core::BytesBuffer(budget_key_timeframe_manager_log.ByteSizeLong());
    auto execution_result = core::common::Serialization::SerializeProtoMessage<
        proto::BudgetKeyTimeframeManagerLog>(
        budget_key_timeframe_log_bytes_buffer, offset,
        budget_key_timeframe_manager_log, bytes_serialized);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }
    budget_key_timeframe_log_bytes_buffer.length = bytes_serialized;
    return core::SuccessExecutionResult();
  }

  /**
   * @brief Deserializes budget key time frame manager object from log.
   *
   * @param budget_key_timeframe_manager_log_bytes_buffer The buffer to read the
   * serialized object from.
   * @param budget_key_timeframe_manager_log The log object to be created from
   * the serialized buffer.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult DeserializeBudgetKeyTimeframeManagerLog(
      const core::BytesBuffer& budget_key_timeframe_manager_log_bytes_buffer,
      proto::BudgetKeyTimeframeManagerLog& budget_key_timeframe_manager_log) {
    // For budget key time frame serialization is a considered as an update
    // operation to the entries in the cache.
    size_t bytes_deserialized = 0;
    size_t offset = 0;
    auto execution_result =
        core::common::Serialization::DeserializeProtoMessage<
            proto::BudgetKeyTimeframeManagerLog>(
            budget_key_timeframe_manager_log_bytes_buffer, offset,
            budget_key_timeframe_manager_log_bytes_buffer.length,
            budget_key_timeframe_manager_log, bytes_deserialized);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    return core::common::Serialization::ValidateVersion(
        budget_key_timeframe_manager_log, kCurrentVersion);
  }

  /**
   * @brief Serializes budget key time frame manager 1_0 log.
   *
   * @param budget_key_timeframe_manager_log_1_0 The log object to serialize.
   * @param budget_key_timeframe_log_bytes_buffer The byte
   * buffer to write the data to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeBudgetKeyTimeframeManagerLog_1_0(
      const proto::BudgetKeyTimeframeManagerLog_1_0&
          budget_key_timeframe_manager_log_1_0,
      core::BytesBuffer& budget_key_timeframe_manager_log_1_0_bytes_buffer) {
    size_t offset = 0;
    size_t bytes_serialized = 0;
    budget_key_timeframe_manager_log_1_0_bytes_buffer =
        core::BytesBuffer(budget_key_timeframe_manager_log_1_0.ByteSizeLong());
    auto execution_result = core::common::Serialization::SerializeProtoMessage<
        proto::BudgetKeyTimeframeManagerLog_1_0>(
        budget_key_timeframe_manager_log_1_0_bytes_buffer, offset,
        budget_key_timeframe_manager_log_1_0, bytes_serialized);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }
    budget_key_timeframe_manager_log_1_0_bytes_buffer.length = bytes_serialized;
    return core::SuccessExecutionResult();
  }

  /**
   * @brief Deserializes budget key time frame manager 1_0 object from log.
   *
   * @param budget_key_timeframe_manager_log_1_0_str The buffer to read the
   * serialized object from.
   * @param budget_key_timeframe_manager_log_1_0 The log object to be created
   * from the serialized buffer.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult DeserializeBudgetKeyTimeframeManagerLog_1_0(
      const std::string& budget_key_timeframe_manager_log_1_0_str,
      proto::BudgetKeyTimeframeManagerLog_1_0&
          budget_key_timeframe_manager_log_1_0) {
    size_t bytes_deserialized = 0;
    return core::common::Serialization::DeserializeProtoMessage<
        proto::BudgetKeyTimeframeManagerLog_1_0>(
        budget_key_timeframe_manager_log_1_0_str,
        budget_key_timeframe_manager_log_1_0, bytes_deserialized);
  }

  /**
   * @brief Serializes the budget key timeframe 1_0 into the provided buffer.
   *
   * @param budget_key_timeframe The budget key timeframe to serialize.
   * @param budget_key_timeframe_log_1_0_bytes_buffer The buffer to write the
   * serialized data to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeBudgetKeyTimeframeLog_1_0(
      const std::shared_ptr<BudgetKeyTimeframe>& budget_key_timeframe,
      core::BytesBuffer& budget_key_timeframe_log_1_0_bytes_buffer) noexcept {
    // Creating the budget key timeframe log v1.0 object.
    proto::BudgetKeyTimeframeLog_1_0 budget_key_timeframe_log_1_0;
    budget_key_timeframe_log_1_0.set_active_token_count(
        budget_key_timeframe->active_token_count);
    budget_key_timeframe_log_1_0.mutable_active_transaction_id()->set_high(
        budget_key_timeframe->active_transaction_id.load().high);
    budget_key_timeframe_log_1_0.mutable_active_transaction_id()->set_low(
        budget_key_timeframe->active_transaction_id.load().low);
    budget_key_timeframe_log_1_0.set_time_bucket(
        budget_key_timeframe->time_bucket_index);
    budget_key_timeframe_log_1_0.set_token_count(
        budget_key_timeframe->token_count);

    // Serializing the budget key timeframe log v1.0 object.
    size_t offset = 0;
    size_t bytes_serialized = 0;
    budget_key_timeframe_log_1_0_bytes_buffer =
        core::BytesBuffer(budget_key_timeframe_log_1_0.ByteSizeLong());
    auto execution_result = core::common::Serialization::SerializeProtoMessage<
        proto::BudgetKeyTimeframeLog_1_0>(
        budget_key_timeframe_log_1_0_bytes_buffer, offset,
        budget_key_timeframe_log_1_0, bytes_serialized);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }
    budget_key_timeframe_log_1_0_bytes_buffer.length = bytes_serialized;
    return core::SuccessExecutionResult();
  }

  /**
   * @brief Serializes the batch budget key timeframe 1_0 into the provided
   * buffer.
   *
   * @param budget_key_timeframes The budget key timeframes to serialize.
   * @param batch_budget_key_timeframe_log_1_0_bytes_buffer The buffer to write
   * the serialized data to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeBatchBudgetKeyTimeframeLog_1_0(
      const std::vector<std::shared_ptr<BudgetKeyTimeframe>>&
          budget_key_timeframes,
      core::BytesBuffer&
          batch_budget_key_timeframe_log_1_0_bytes_buffer) noexcept {
    if (budget_key_timeframes.empty()) {
      return core::FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_INVALID_LOG);
    }

    // Creating the batch budget key timeframe log v1.0 object.
    proto::BatchBudgetKeyTimeframeLog_1_0 batch_budget_key_timeframe_log_1_0;
    for (const auto& budget_key_timeframe : budget_key_timeframes) {
      auto budget_key_timeframe_log_1_0 =
          batch_budget_key_timeframe_log_1_0.add_items();
      budget_key_timeframe_log_1_0->set_active_token_count(
          budget_key_timeframe->active_token_count);
      budget_key_timeframe_log_1_0->mutable_active_transaction_id()->set_high(
          budget_key_timeframe->active_transaction_id.load().high);
      budget_key_timeframe_log_1_0->mutable_active_transaction_id()->set_low(
          budget_key_timeframe->active_transaction_id.load().low);
      budget_key_timeframe_log_1_0->set_time_bucket(
          budget_key_timeframe->time_bucket_index);
      budget_key_timeframe_log_1_0->set_token_count(
          budget_key_timeframe->token_count);
    }

    // Serializing the batch budget key timeframe log v1.0 object.
    size_t offset = 0;
    size_t bytes_serialized = 0;
    batch_budget_key_timeframe_log_1_0_bytes_buffer =
        core::BytesBuffer(batch_budget_key_timeframe_log_1_0.ByteSizeLong());
    auto execution_result = core::common::Serialization::SerializeProtoMessage<
        proto::BatchBudgetKeyTimeframeLog_1_0>(
        batch_budget_key_timeframe_log_1_0_bytes_buffer, offset,
        batch_budget_key_timeframe_log_1_0, bytes_serialized);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }
    batch_budget_key_timeframe_log_1_0_bytes_buffer.length = bytes_serialized;
    return core::SuccessExecutionResult();
  }

  /**
   * @brief Deserializes budget key time frame 1_0 object from log.
   *
   * @param budget_key_timeframe_log_1_0_str The buffer to read the
   * serialized object from.
   * @param budget_key_timeframe The log object to be created
   * from the serialized buffer.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult DeserializeBudgetKeyTimeframeLog_1_0(
      const std::string& budget_key_timeframe_log_1_0_str,
      std::shared_ptr<BudgetKeyTimeframe>& budget_key_timeframe) noexcept {
    proto::BudgetKeyTimeframeLog_1_0 budget_key_timeframe_log_1_0;
    size_t bytes_deserialized = 0;

    auto execution_result =
        core::common::Serialization::DeserializeProtoMessage<
            proto::BudgetKeyTimeframeLog_1_0>(budget_key_timeframe_log_1_0_str,
                                              budget_key_timeframe_log_1_0,
                                              bytes_deserialized);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    budget_key_timeframe = std::make_shared<BudgetKeyTimeframe>(
        budget_key_timeframe_log_1_0.time_bucket());
    budget_key_timeframe->active_token_count =
        budget_key_timeframe_log_1_0.active_token_count();
    budget_key_timeframe->token_count =
        budget_key_timeframe_log_1_0.token_count();
    core::common::Uuid active_transaction_id;
    active_transaction_id.high =
        budget_key_timeframe_log_1_0.active_transaction_id().high();
    active_transaction_id.low =
        budget_key_timeframe_log_1_0.active_transaction_id().low();
    budget_key_timeframe->active_transaction_id = active_transaction_id;
    return core::SuccessExecutionResult();
  }

  /**
   * @brief Deserializes batch budget key time frame 1_0 object from log.
   *
   * @param batch_budget_key_timeframe_log_1_0_str The buffer to read the
   * serialized object from.
   * @param budget_key_timeframes The time frame objects to be created
   * from the serialized buffer.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult DeserializeBatchBudgetKeyTimeframeLog_1_0(
      const std::string& batch_budget_key_timeframe_log_1_0_str,
      std::vector<std::shared_ptr<BudgetKeyTimeframe>>&
          budget_key_timeframes) noexcept {
    proto::BatchBudgetKeyTimeframeLog_1_0 batch_budget_key_timeframe_log_1_0;
    size_t bytes_deserialized = 0;

    if (batch_budget_key_timeframe_log_1_0_str.empty()) {
      return core::FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_INVALID_LOG);
    }

    auto execution_result =
        core::common::Serialization::DeserializeProtoMessage<
            proto::BatchBudgetKeyTimeframeLog_1_0>(
            batch_budget_key_timeframe_log_1_0_str,
            batch_budget_key_timeframe_log_1_0, bytes_deserialized);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    if (batch_budget_key_timeframe_log_1_0.items().empty()) {
      return core::FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_INVALID_LOG);
    }

    for (const auto& budget_key_timeframe_log_1_0 :
         batch_budget_key_timeframe_log_1_0.items()) {
      auto budget_key_timeframe = std::make_shared<BudgetKeyTimeframe>(
          budget_key_timeframe_log_1_0.time_bucket());
      budget_key_timeframe->active_token_count =
          budget_key_timeframe_log_1_0.active_token_count();
      budget_key_timeframe->token_count =
          budget_key_timeframe_log_1_0.token_count();
      core::common::Uuid active_transaction_id;
      active_transaction_id.high =
          budget_key_timeframe_log_1_0.active_transaction_id().high();
      active_transaction_id.low =
          budget_key_timeframe_log_1_0.active_transaction_id().low();
      budget_key_timeframe->active_transaction_id = active_transaction_id;
      budget_key_timeframes.push_back(budget_key_timeframe);
    }
    return core::SuccessExecutionResult();
  }

  /**
   * @brief Serializes the budget key timeframe group into the provided
   * buffer.
   *
   * @param budget_key_timeframe_group The budget key timeframe group to
   * serialize.
   * @param budget_key_timeframe_group_log_bytes_buffer The buffer to write the
   * serialized data to.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeBudgetKeyTimeframeGroupLog_1_0(
      const std::shared_ptr<BudgetKeyTimeframeGroup>&
          budget_key_timeframe_group,
      core::BytesBuffer& budget_key_timeframe_group_log_bytes_buffer) noexcept {
    proto::BudgetKeyTimeframeGroupLog_1_0 budget_key_timeframe_group_log_1_0;
    budget_key_timeframe_group_log_1_0.set_time_group(
        budget_key_timeframe_group->time_group);
    std::vector<TimeBucket> time_buckets;
    auto execution_result =
        budget_key_timeframe_group->budget_key_timeframes.Keys(time_buckets);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    for (auto time_bucket : time_buckets) {
      std::shared_ptr<BudgetKeyTimeframe> budget_key_timeframe;
      execution_result = budget_key_timeframe_group->budget_key_timeframes.Find(
          time_bucket, budget_key_timeframe);
      if (execution_result != core::SuccessExecutionResult()) {
        return execution_result;
      }

      auto loaded_frame = budget_key_timeframe_group_log_1_0.add_items();
      loaded_frame->set_token_count(budget_key_timeframe->token_count);
      loaded_frame->set_time_bucket(budget_key_timeframe->time_bucket_index);
      loaded_frame->set_active_token_count(
          budget_key_timeframe->active_token_count);
      loaded_frame->set_time_bucket(budget_key_timeframe->time_bucket_index);
      loaded_frame->mutable_active_transaction_id()->set_high(
          budget_key_timeframe->active_transaction_id.load().high);
      loaded_frame->mutable_active_transaction_id()->set_low(
          budget_key_timeframe->active_transaction_id.load().low);
    }

    size_t offset = 0;
    size_t bytes_serialized = 0;
    budget_key_timeframe_group_log_bytes_buffer =
        core::BytesBuffer(budget_key_timeframe_group_log_1_0.ByteSizeLong());
    execution_result = core::common::Serialization::SerializeProtoMessage<
        proto::BudgetKeyTimeframeGroupLog_1_0>(
        budget_key_timeframe_group_log_bytes_buffer, offset,
        budget_key_timeframe_group_log_1_0, bytes_serialized);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }
    budget_key_timeframe_group_log_bytes_buffer.length = bytes_serialized;
    return core::SuccessExecutionResult();
  }

  /**
   * @brief Deserializes the budget key timeframe group from the provided
   * buffer.
   *
   * @param budget_key_timeframe_group_log_str The buffer containing the
   * serialized object.
   * @param budget_key_timeframe_group The pointer to create the object from the
   * serialized data.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult DeserializeBudgetKeyTimeframeGroupLog_1_0(
      const std::string& budget_key_timeframe_group_log_str,
      std::shared_ptr<BudgetKeyTimeframeGroup>&
          budget_key_timeframe_group) noexcept {
    proto::BudgetKeyTimeframeGroupLog_1_0 budget_key_timeframe_group_log_1_0;
    if (budget_key_timeframe_group_log_str.length() == 0) {
      return core::FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA);
    }

    size_t bytes_deserialized = 0;
    auto execution_result =
        core::common::Serialization::DeserializeProtoMessage<
            proto::BudgetKeyTimeframeGroupLog_1_0>(
            budget_key_timeframe_group_log_str,
            budget_key_timeframe_group_log_1_0, bytes_deserialized);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    budget_key_timeframe_group = std::make_shared<BudgetKeyTimeframeGroup>(
        budget_key_timeframe_group_log_1_0.time_group());

    auto budget_key_timeframe_group_pair =
        std::make_pair(budget_key_timeframe_group_log_1_0.time_group(),
                       budget_key_timeframe_group);

    for (int items_index = 0;
         items_index < budget_key_timeframe_group_log_1_0.items().size();
         ++items_index) {
      auto item = budget_key_timeframe_group_log_1_0.items(items_index);
      auto budget_key_timeframe =
          std::make_shared<BudgetKeyTimeframe>(item.time_bucket());
      auto budget_key_timeframe_pair =
          std::make_pair(item.time_bucket(), budget_key_timeframe);
      auto execution_result =
          budget_key_timeframe_group->budget_key_timeframes.Insert(
              budget_key_timeframe_pair, budget_key_timeframe);
      if (execution_result != core::SuccessExecutionResult()) {
        if (execution_result.status_code !=
            core::errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS) {
          return execution_result;
        }
      }
      budget_key_timeframe->token_count = item.token_count();

      core::common::Uuid active_transaction_id;
      active_transaction_id.high = item.active_transaction_id().high();
      active_transaction_id.low = item.active_transaction_id().low();

      budget_key_timeframe->active_transaction_id = active_transaction_id;
      budget_key_timeframe->active_token_count = item.active_token_count();
    }

    return core::SuccessExecutionResult();
  }

  /**
   * @brief Serializes 24 hours token per hour vector into string.
   *
   * @param hour_tokens A vector with size of 24. Each index represents the
   * amount of tokens available in a hour.
   * @param hour_token_in_time_group The serialized buffer of the vector.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeHourTokensInTimeGroup(
      const std::vector<TokenCount>& hour_tokens,
      std::string& hour_token_in_time_group) {
    if (hour_tokens.size() != kHoursPerDay) {
      return core::FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA);
    }

    hour_token_in_time_group.clear();
    for (size_t i = 0; i < hour_tokens.size(); ++i) {
      hour_token_in_time_group += std::to_string(hour_tokens[i]);
      if (i < hour_tokens.size() - 1) {
        hour_token_in_time_group += " ";
      }
    }

    return core::SuccessExecutionResult();
  }

  /**
   * @brief Deserializes 24 hours token per hour vector from the provided
   * string.
   *
   * @param hour_token_in_time_group The serialized buffer of the vector.
   * @param hour_tokens A vector with size of 24. Each index represents the
   * amount of tokens available in a hour.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult DeserializeHourTokensInTimeGroup(
      const std::string& hour_token_in_time_group,
      std::vector<TokenCount>& hour_tokens) {
    std::vector<std::string> tokens_per_hour;
    boost::algorithm::split(tokens_per_hour, hour_token_in_time_group,
                            boost::algorithm::is_any_of(" "),
                            boost::algorithm::token_compress_on);

    if (tokens_per_hour.size() != kHoursPerDay) {
      return core::FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA);
    }

    hour_tokens.reserve(kHoursPerDay);
    for (auto token_per_hour : tokens_per_hour) {
      try {
        auto value = stoi(token_per_hour);
        hour_tokens.push_back(value);
      } catch (...) {
        return core::FailureExecutionResult(
            core::errors::
                SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA);
      }
    }

    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::pbs::budget_key_timeframe_manager
