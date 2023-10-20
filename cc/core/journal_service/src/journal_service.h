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
#include <unordered_set>

#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/partition_types.h"
#include "core/journal_service/interface/journal_service_stream_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/simple_metric_interface.h"

// TODO: Make the retry strategy configurable.
static constexpr google::scp::core::TimeDuration
    kJournalServiceRetryStrategyDelayMs = 31;
static constexpr size_t kJournalServiceRetryStrategyTotalRetries = 12;

namespace google::scp::core {
/*! @copydoc JournalServiceInterface
 */
class JournalService : public JournalServiceInterface {
 public:
  JournalService(
      const std::shared_ptr<std::string>& bucket_name,
      const std::shared_ptr<std::string>& partition_name,
      const std::shared_ptr<AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<BlobStorageProviderInterface>&
          blob_storage_provider,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<ConfigProviderInterface>& config_provider)
      : is_initialized_(false),
        is_running_(false),
        bucket_name_(bucket_name),
        partition_name_(partition_name),
        async_executor_(async_executor),
        blob_storage_provider_(blob_storage_provider),
        operation_dispatcher_(
            async_executor,
            common::RetryStrategy(common::RetryStrategyType::Exponential,
                                  kJournalServiceRetryStrategyDelayMs,
                                  kJournalServiceRetryStrategyTotalRetries)),
        metric_client_(metric_client),
        config_provider_(config_provider),
        journal_flush_interval_in_milliseconds_(0) {}

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ExecutionResult RunRecoveryMetrics() noexcept override;

  ExecutionResult StopRecoveryMetrics() noexcept override;

  ExecutionResult Log(AsyncContext<JournalLogRequest, JournalLogResponse>&
                          journal_log_context) noexcept override;

  ExecutionResult Recover(
      AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
          journal_recover_context) noexcept override;

  ExecutionResult SubscribeForRecovery(
      const common::Uuid& component_id,
      OnLogRecoveredCallback callback) noexcept override;

  ExecutionResult UnsubscribeForRecovery(
      const common::Uuid& component_id) noexcept override;

  ExecutionResult GetLastPersistedJournalId(
      JournalId& journal_id) noexcept override;

 protected:
  /**
   * @brief Is called after the read log operation is completed.
   *
   * @param time_event An instance of time event to record event start, end
   * time.
   * @param metric_instance An instance of simple metric.
   * @param replayed_logs An unordered set of replayed logs to ensure the same
   * log will not be played twice.
   * @param journal_recover_context The context of the recovery operation.
   * @param journal_stream_read_log_context The context of the journal stream
   * read operation.
   */
  virtual void OnJournalStreamReadLogCallback(
      std::shared_ptr<cpio::TimeEvent>& time_event,
      std::shared_ptr<std::unordered_set<std::string>>& replayed_logs,
      AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
          journal_recover_context,
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          journal_stream_read_log_context) noexcept;

  /**
   * @brief Is called after append log operation is completed.
   *
   * @param journal_context The context of the journal log operation.
   * @param journal_stream_append_log_context The context of the journal
   * stream append log operation.
   */
  virtual void OnJournalStreamAppendLogCallback(
      AsyncContext<JournalLogRequest, JournalLogResponse>& journal_log_context,
      AsyncContext<journal_service::JournalStreamAppendLogRequest,
                   journal_service::JournalStreamAppendLogResponse>&
          write_journal_stream_context) noexcept;

  /// Flushes the current output stream.
  virtual void FlushJournalOutputStream() noexcept;

  bool is_running() {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_running_;
  }

  /// Indicates whether the journal service is initialized.
  bool is_initialized_;

  /// Indicates whether the journal service is running.
  std::mutex mutex_;
  bool is_running_;  // protected by mutex_

  /// The current bucket name to write the blob to.
  std::shared_ptr<std::string> bucket_name_;

  /// The current partition name to write the blob to.
  std::shared_ptr<std::string> partition_name_;

  /// Async executor instance.
  std::shared_ptr<AsyncExecutorInterface> async_executor_;

  /// An instance of the blob storage provider.
  std::shared_ptr<BlobStorageProviderInterface> blob_storage_provider_;

  /// Blob storage provider client instance.
  std::shared_ptr<BlobStorageClientInterface> blob_storage_provider_client_;

  /// An instance of the journal input stream.
  std::shared_ptr<journal_service::JournalInputStreamInterface>
      journal_input_stream_;

  /// An instance of the journal output stream.
  std::shared_ptr<journal_service::JournalOutputStreamInterface>
      journal_output_stream_;

  /// Operation dispatcher
  common::OperationDispatcher operation_dispatcher_;

  /// The map of all the subscribers to the journal service.
  common::ConcurrentMap<common::Uuid, OnLogRecoveredCallback,
                        common::UuidCompare>
      subscribers_map_;

  /// Metric client instance for custom metric recording.
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;

  /// The simple metric instance for journal service recovery time.
  std::shared_ptr<cpio::SimpleMetricInterface> recover_time_metric_;

  /// The aggregate metric instance for journal service recovery log count while
  /// recovering.
  std::shared_ptr<cpio::AggregateMetricInterface> recover_log_count_metric_;

  /// The aggregate metric instance for journal service output stream count
  /// while running.
  std::shared_ptr<cpio::AggregateMetricInterface> journal_output_count_metric_;

  /// A unique pointer to the working thread.
  std::unique_ptr<std::thread> flushing_thread_;

  /// Config provider
  std::shared_ptr<ConfigProviderInterface> config_provider_;

  /// Encapsulating Partition ID
  PartitionId partition_id_;

  /// Journal flush interval
  size_t journal_flush_interval_in_milliseconds_;
};
}  // namespace google::scp::core
