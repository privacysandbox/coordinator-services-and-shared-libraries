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

#include <exception>
#include <string>

namespace google::scp::roma::common {
/** A shared memory mutex that meets the std "Lockable" requirement.
 * https://en.cppreference.com/w/cpp/named_req/Lockable
 * Please note that the construction of objects of this class has to be
 * carefully done in shared memory in order for synchronization to work across
 * processes. This mutex implementation is non-recursive.
 */
template <bool Recursive>
class ShmMutexBase {
 public:
  /// Default constructs a ShmMutex. Please note: to make it work across
  /// processes, it has to reside in a shared memory region.
  ShmMutexBase() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if constexpr (Recursive) {
      pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }
    pthread_mutex_init(&mutex_, &attr);
  }

  /// Blocks while trying to acquire the mutex.
  void lock() {
    int ret = pthread_mutex_lock(&mutex_);
    // There is no way to know the failure reason by this signature. So we just
    // crash if not success.
    if (ret != 0) {
      throw std::runtime_error(
          std::string("Failed to lock mutex due to internal errno=") +
          std::to_string(errno));
    }
  }

  /// Try to acquire the mutex, returns immediately regardless of success.
  /// @return true of successful.
  bool try_lock() {
    int ret = pthread_mutex_trylock(&mutex_);
    if (ret == 0) {
      return true;
    }
    // There is no way to know the failure reason by this signature. So we just
    // crash if not graceful failure.
    if (errno != EBUSY || errno != EAGAIN) {
      throw std::runtime_error(
          std::string("Failed to try_lock mutex due to internal errno=") +
          std::to_string(errno));
    }
    return false;
  }

  /// Unlocks a locked mutex. Caller must own the mutex.
  void unlock() {
    int ret = pthread_mutex_unlock(&mutex_);
    // There is no way to know the failure reason by this signature. So we just
    // crash if not success.
    if (ret != 0) {
      throw std::runtime_error(
          std::string("Failed to unlock mutex due to internal errno=") +
          std::to_string(errno));
    }
  }

 private:
  pthread_mutex_t mutex_;
};

using ShmMutex = ShmMutexBase<false>;

using RecursiveShmMutex = ShmMutexBase<true>;
}  // namespace google::scp::roma::common
