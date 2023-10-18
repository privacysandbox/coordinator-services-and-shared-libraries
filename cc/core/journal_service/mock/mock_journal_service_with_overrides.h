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

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>

#include "core/interface/config_provider_interface.h"
#include "core/journal_service/src/journal_service.h"
#include "public/cpio/utils/metric_aggregation/mock/mock_aggregate_metric.h"
#include "public/cpio/utils/metric_aggregation/mock/mock_simple_metric.h"

namespace google::scp::core::journal_service::mock {
class MockJournalServiceWithOverrides : public JournalService {
 public:
  MockJournalServiceWithOverrides(
      const std::shared_ptr<std::string>& bucket_name,
      const std::shared_ptr<std::string>& partition_name,
      const std::shared_ptr<AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<BlobStorageProviderInterface>&
          blob_storage_provider,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<ConfigProviderInterface>& config_provider)
      : JournalService(bucket_name, partition_name, async_executor,
                       blob_storage_provider, metric_client, config_provider) {
    // TODO: Temporary hack to make the tests work. This will be removed when we
    // have a MetricFactory (we can just simply use MockMetricFactory)
    recover_time_metric_ = std::make_shared<cpio::MockSimpleMetric>();
    recover_log_count_metric_ = std::make_shared<cpio::MockAggregateMetric>();
    journal_output_count_metric_ =
        std::make_shared<cpio::MockAggregateMetric>();
  }

  void SetInputStream(
      std::shared_ptr<journal_service::JournalInputStreamInterface>&
          input_stream) {
    journal_input_stream_ = input_stream;
  }

  std::shared_ptr<journal_service::JournalInputStreamInterface>
  GetInputStream() {
    return journal_input_stream_;
  }

  void SetOutputStream(
      std::shared_ptr<journal_service::JournalOutputStreamInterface>&
          output_stream) {
    journal_output_stream_ = output_stream;
  }

  std::shared_ptr<journal_service::JournalOutputStreamInterface>
  GetOutputStream() {
    return journal_output_stream_;
  }

  common::ConcurrentMap<common::Uuid, OnLogRecoveredCallback,
                        common::UuidCompare>&
  GetSubscribersMap() {
    return subscribers_map_;
  }

  virtual void OnJournalStreamReadLogCallback(
      std::shared_ptr<cpio::TimeEvent>& time_event,
      std::shared_ptr<std::unordered_set<std::string>>& replayed_logs,
      AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
          journal_recover_context,
      AsyncContext<journal_service::JournalStreamReadLogRequest,
                   journal_service::JournalStreamReadLogResponse>&
          journal_stream_read_log_context) noexcept {
    JournalService::OnJournalStreamReadLogCallback(
        time_event, replayed_logs, journal_recover_context,
        journal_stream_read_log_context);
  }

  virtual void OnJournalStreamAppendLogCallback(
      AsyncContext<JournalLogRequest, JournalLogResponse>& journal_log_context,
      AsyncContext<journal_service::JournalStreamAppendLogRequest,
                   journal_service::JournalStreamAppendLogResponse>&
          write_journal_stream_context) noexcept {
    JournalService::OnJournalStreamAppendLogCallback(
        journal_log_context, write_journal_stream_context);
  }
};
}  // namespace google::scp::core::journal_service::mock
