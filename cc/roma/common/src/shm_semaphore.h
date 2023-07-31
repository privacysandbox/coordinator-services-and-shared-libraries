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

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdint.h>

#include "error_codes.h"

namespace google::scp::roma::common {
/**
 * A process-shared semaphore, which is just a wrapper of POSIX semaphore to
 * provide synchronization mechanism over shared memory.
 */
class ShmSemaphore {
 public:
  explicit ShmSemaphore(uint32_t init_count) {
    sem_init(&semaphore_, 1 /* pshared */, init_count);
    // TODO: handle errors of sem_init
  }

  ~ShmSemaphore() { sem_destroy(&semaphore_); }

  /// Wait for one signal, block until one available.
  core::ExecutionResult WaitOne() {
    int rc;
    do {
      errno = 0;
      rc = sem_wait(&semaphore_);
    } while (rc < 0 && errno == EINTR);
    if (rc == 0) {
      return core::SuccessExecutionResult();
    }
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_SEMAPHORE_BAD_SEMAPHORE);
  }

  /// Wait for \a count signals, block until wait succeeds or hits error.
  core::ExecutionResult Wait(uint32_t count = 1) {
    while (count--) {
      auto result = WaitOne();
      if (!result.Successful()) {
        return result;
      }
    }
    return core::SuccessExecutionResult();
  }

  /// Try to wait for one signal. Immediately returns.
  core::ExecutionResult TryWait() {
    int rc;
    do {
      errno = 0;
      rc = sem_trywait(&semaphore_);
    } while (rc < 0 && errno == EINTR);
    if (rc == 0) {
      return core::SuccessExecutionResult();
    }
    if (errno == EAGAIN) {
      return core::FailureExecutionResult(
          core::errors::SC_ROMA_SEMAPHORE_WOULD_BLOCK);
    }
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_SEMAPHORE_BAD_SEMAPHORE);
  }

  /// The current value of the semaphore. Note the linux implementation returns
  /// 0 when there are active waiters, so this cannot be used as indication of
  /// number of waiters.
  core::ExecutionResult Value(int& value) {
    int rc = sem_getvalue(&semaphore_, &value);
    if (rc < 0) {
      return core::FailureExecutionResult(
          core::errors::SC_ROMA_SEMAPHORE_BAD_SEMAPHORE);
    }
    return core::SuccessExecutionResult();
  }

  /// Signal one.
  core::ExecutionResult Signal() {
    int rc;
    do {
      rc = sem_post(&semaphore_);
    } while (rc < 0 && errno == EOVERFLOW);
    if (rc == 0) {
      return core::SuccessExecutionResult();
    }
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_SEMAPHORE_BAD_SEMAPHORE);
  }

  /// Signal \a count times.
  core::ExecutionResult Signal(uint32_t count) {
    while (count--) {
      auto result = Signal();
      if (!result.Successful()) {
        return result;
      }
    }
  }

 private:
  // TODO: add atomic counter to avoid going through syscall every time we
  // signal or wait.
  sem_t semaphore_;
};
}  // namespace google::scp::roma::common
