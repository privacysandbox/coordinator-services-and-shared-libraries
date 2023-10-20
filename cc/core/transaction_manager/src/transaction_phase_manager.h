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

#include "core/transaction_manager/interface/transaction_phase_manager_interface.h"

namespace google::scp::core {
/*! @copydoc TransactionManagerInterface
 */
class TransactionPhaseManager
    : public transaction_manager::TransactionPhaseManagerInterface {
 public:
  transaction_manager::TransactionPhase ProceedToNextPhase(
      transaction_manager::TransactionPhase current_phase,
      ExecutionResult current_phase_result) noexcept override;

  /**
   * @brief Helper function to determine if transaction can be moved to Abort
   * phase from the current phase
   *
   * @param phase
   * @return true
   * @return false
   */
  static bool CanProceedToAbortAtPhase(
      transaction_manager::TransactionPhase phase);

  /**
   * @brief Helper function to determine if transaction can be moved to End
   * phase from the current phase
   *
   * @param phase
   * @return true
   * @return false
   */
  static bool CanProceedToEndAtPhase(
      transaction_manager::TransactionPhase phase);

 private:
  transaction_manager::TransactionPhase ProceedToNextPhaseInternal(
      transaction_manager::TransactionPhase current_phase,
      bool current_phase_succeeded) noexcept;
};
}  // namespace google::scp::core
