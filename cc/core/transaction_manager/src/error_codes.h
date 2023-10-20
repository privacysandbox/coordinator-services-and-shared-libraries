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

#include "cc/core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {
/// Registers component code as 0x0007 for transaction manager.
REGISTER_COMPONENT_CODE(SC_TRANSACTION_MANAGER, 0x0007)

DEFINE_ERROR_CODE(
    SC_TRANSACTION_MANAGER_CANNOT_INITIALIZE, SC_TRANSACTION_MANAGER, 0x0001,
    "Cannot initialize the transaction manager after it has started.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_ALREADY_STARTED,
                  SC_TRANSACTION_MANAGER, 0x0002,
                  "Transaction manager has already started.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_NOT_STARTED, SC_TRANSACTION_MANAGER,
                  0x0003, "Transaction manager has not started.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_CANNOT_ACCEPT_NEW_REQUESTS,
                  SC_TRANSACTION_MANAGER, 0x0004,
                  "Transaction manager cannot accept new requests.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(
    SC_TRANSACTION_MANAGER_INVALID_MAX_CONCURRENT_TRANSACTIONS_VALUE,
    SC_TRANSACTION_MANAGER, 0x0006, "Invalid max concurrent transaction value.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_ENGINE_CANNOT_ACCEPT_TRANSACTION,
                  SC_TRANSACTION_MANAGER, 0x0007,
                  "Transaction engine cannot accept new transactions.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_TRANSACTION_ENGINE_INVALID_DISTRIBUTED_COMMAND,
                  SC_TRANSACTION_MANAGER, 0x0008,
                  "Invalid distributed transaction command.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_ALREADY_STOPPED,
                  SC_TRANSACTION_MANAGER, 0x0009,
                  "Transaction manager has already stopped.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_NOT_FINISHED, SC_TRANSACTION_MANAGER,
                  0x000A, "Transaction is not finished.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND,
                  SC_TRANSACTION_MANAGER, 0x000B,
                  "The transaction is not found.", HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_LOG,
                  SC_TRANSACTION_MANAGER, 0x000C,
                  "The transaction log is invalid.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TERMINATABLE_TRANSACTION,
                  SC_TRANSACTION_MANAGER, 0x000D,
                  "The transaction is terminatable.",
                  HttpStatusCode::MOVED_PERMANENTLY)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_INVALID_TRANSACTION_PHASE,
                  SC_TRANSACTION_MANAGER, 0x000E,
                  "The transaction current phases is not valid.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_CURRENT_TRANSACTION_IS_RUNNING,
                  SC_TRANSACTION_MANAGER, 0x000F,
                  "The transaction is currently running so the requested phase "
                  "cannot be executed at this time.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(
    SC_TRANSACTION_MANAGER_CANNOT_CREATE_CHECKPOINT_WHEN_STARTED,
    SC_TRANSACTION_MANAGER, 0x0010,
    "Cannot create checkpoint if the transaction manager is running.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_TIMEOUT,
                  SC_TRANSACTION_MANAGER, 0x0011,
                  "The transaction has timed out.",
                  HttpStatusCode::REQUEST_TIMEOUT)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_NOT_COORDINATED_REMOTELY,
                  SC_TRANSACTION_MANAGER, 0x0012,
                  "The transaction is not coordinated remotely.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_SECRET_IS_NOT_VALID,
                  SC_TRANSACTION_MANAGER, 0x0013,
                  "The transaction secret is not valid.",
                  HttpStatusCode::UNAUTHORIZED)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_IS_EXPIRED,
                  SC_TRANSACTION_MANAGER, 0x0014, "The transaction is expired.",
                  HttpStatusCode::GONE)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_REMOTE_TRANSACTION_FAILED,
                  SC_TRANSACTION_MANAGER, 0x0015,
                  "The remote transaction is failed.", HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_IS_ABORTED,
                  SC_TRANSACTION_MANAGER, 0x0016, "The transaction is aborted.",
                  HttpStatusCode::GONE)

DEFINE_ERROR_CODE(
    SC_TRANSACTION_MANAGER_TRANSACTION_HAS_PENDING_CALLBACKS,
    SC_TRANSACTION_MANAGER, 0x0017,
    "The transaction cannot be resolved due to pending callbacks.",
    HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(
    SC_TRANSACTION_MANAGER_TRANSACTION_CANNOT_BE_LOCKED, SC_TRANSACTION_MANAGER,
    0x0018,
    "The transaction cannot be blocked for having consensus with remote.",
    HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_ORIGIN_IS_NOT_VALID,
                  SC_TRANSACTION_MANAGER, 0x0019,
                  "The remote transaction origin is invalid.",
                  HttpStatusCode::UNAUTHORIZED)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_OUT_OF_SYNC,
                  SC_TRANSACTION_MANAGER, 0x001A,
                  "The transaction and the remote transaction are out of sync.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_STUCK,
                  SC_TRANSACTION_MANAGER, 0x001B,
                  "The transaction is stuck and requires manual recovery.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_UNKNOWN,
                  SC_TRANSACTION_MANAGER, 0x001C,
                  "The transaction stuck at an unknown phase.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_LAST_EXECUTION_TIMESTAMP_NOT_MATCHING,
                  SC_TRANSACTION_MANAGER, 0x001D,
                  "The last execution timestamp of transaction does not match.",
                  HttpStatusCode::PRECONDITION_FAILED)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_CANNOT_BE_UNLOCKED,
                  SC_TRANSACTION_MANAGER, 0x001E,
                  "The transaction cannot be unblocked.",
                  HttpStatusCode::CONFLICT)

DEFINE_ERROR_CODE(SC_TRANSACTION_ENGINE_INVALID_TRANSACTION_REQUEST,
                  SC_TRANSACTION_MANAGER, 0x001F,
                  "The transaction request is invalid.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_STATUS_CANNOT_BE_OBTAINED,
                  SC_TRANSACTION_MANAGER, 0x0020,
                  "Transaction Manager's get status cannot be obtained at this "
                  "time due to internal service unavailability.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_QUEUE_FAILURE, SC_TRANSACTION_MANAGER,
                  0x0021, "Transaction Manager's queue has an issue.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_STATE_IS_INVALID,
                  SC_TRANSACTION_MANAGER, 0x0022,
                  "Transaction Manager's state is invalid.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_IS_NOT_LOCKED,
                  SC_TRANSACTION_MANAGER, 0x0023,
                  "The remote transaction must be in a locked state to "
                  "execute a phase.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_TRANSACTION_MANAGER_TRANSACTION_ALREADY_EXISTS,
                  SC_TRANSACTION_MANAGER, 0x0024,
                  "The entry already exists in the transaction map.",
                  HttpStatusCode::PRECONDITION_FAILED)

}  // namespace google::scp::core::errors
