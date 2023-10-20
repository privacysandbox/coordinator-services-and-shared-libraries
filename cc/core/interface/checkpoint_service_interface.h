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

#include "core/common/uuid/src/uuid.h"

#include "async_context.h"
#include "journal_service_interface.h"
#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {
/// Represents the checkpoint log.
struct CheckpointLog {
  /// The component id of the checkpointing request.
  common::Uuid component_id;
  /// The id of the log.
  common::Uuid log_id;
  /// The status of the current journal log.
  JournalLogStatus log_status;
  /// The serialized data the need to be stored.
  BytesBuffer bytes_buffer;
};

/**
 * @brief Represents checkpoint service interface. The checkpoint service is a
 * background service responsible to collect the journal logs and create
 * checkpoint files. This service will start with the Run function and keeps
 * checkpointing throughout the lifetime of the application.
 */
class CheckpointServiceInterface : public ServiceInterface {
 public:
  virtual ~CheckpointServiceInterface() = default;

  /**
   * @brief Returns the last persisted checkpoint id by the current checkpoint
   * service.
   *
   * @return ExecutionResultOr<CheckpointId> CheckpointId that was last
   * persisted
   */
  virtual ExecutionResultOr<CheckpointId>
  GetLastPersistedCheckpointId() noexcept = 0;
};
}  // namespace google::scp::core
