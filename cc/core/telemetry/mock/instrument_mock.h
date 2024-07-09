/*
 * Copyright 2024 Google LLC
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
#ifndef CC_CORE_TELEMETRY_MOCK_INSTRUMENT_MOCK
#define CC_CORE_TELEMETRY_MOCK_INSTRUMENT_MOCK

#include "absl/container/flat_hash_map.h"
#include "gmock/gmock.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/metrics/meter.h"

namespace google::scp::core {

template <typename T>
class MockCounter : public opentelemetry::metrics::Counter<T> {
 public:
  MOCK_METHOD(void, Add, (T value), (noexcept, override));
  MOCK_METHOD(void, Add,
              (T value, const opentelemetry::context::Context& context),
              (noexcept, override));
  MOCK_METHOD(void, Add,
              (T value,
               const opentelemetry::common::KeyValueIterable& attributes),
              (noexcept, override));
  MOCK_METHOD(void, Add,
              (T value,
               const opentelemetry::common::KeyValueIterable& attributes,
               const opentelemetry::context::Context& context),
              (noexcept, override));
};

template <typename T>
class MockHistogram : public opentelemetry::metrics::Histogram<T> {
 public:
  MOCK_METHOD(void, Record, (T value), (noexcept, override));
  MOCK_METHOD(void, Record,
              (T value,
               const opentelemetry::common::KeyValueIterable& attribute),
              (noexcept, override));
  MOCK_METHOD(void, Record,
              (T value, const opentelemetry::context::Context& context),
              (noexcept, override));
  MOCK_METHOD(void, Record,
              (T value,
               const opentelemetry::common::KeyValueIterable& attributes,
               const opentelemetry::context::Context& context),
              (noexcept, override));
};

template <typename T>
class MockUpDownCounter : public opentelemetry::metrics::UpDownCounter<T> {
 public:
  MOCK_METHOD(void, Add, (T value), (noexcept, override));
  MOCK_METHOD(void, Add,
              (T value, const opentelemetry::context::Context& context),
              (noexcept, override));
  MOCK_METHOD(void, Add,
              (T value,
               const opentelemetry::common::KeyValueIterable& attributes),
              (noexcept, override));
  MOCK_METHOD(void, Add,
              (T value,
               const opentelemetry::common::KeyValueIterable& attributes,
               const opentelemetry::context::Context& context),
              (noexcept, override));
};
}  // namespace google::scp::core

#endif  // CC_CORE_TELEMETRY_MOCK_INSTRUMENT_MOCK
