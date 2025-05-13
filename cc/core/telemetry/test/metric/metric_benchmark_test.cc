//  Copyright 2025 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include <memory>

#include <benchmark/benchmark.h>

#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/telemetry/mock/in_memory_metric_router.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/view/view_registry_factory.h"

namespace privacy_sandbox::pbs_common {
namespace {

// Benchmarking 1: SpinLock counter increment.
void BM_SpinLockIncrement(benchmark::State& state) {
  static opentelemetry::common::SpinLockMutex lock;
  static int counter = 0;  // Shared counter across threads.

  for (auto _ : state) {
    lock.lock();
    ++counter;  // Increment shared counter.
    lock.unlock();
    benchmark::DoNotOptimize(counter);
  }
}

// Benchmarking 2: Atomic counter increment.
void BM_AtomicIncrement(benchmark::State& state) {
  static std::atomic<int64_t> counter{0};

  for (auto _ : state) {
    counter.fetch_add(1, std::memory_order_relaxed);
    benchmark::DoNotOptimize(counter);
  }
}

std::shared_ptr<opentelemetry::metrics::Counter<uint64_t>> CreateOtelCounter() {
  auto metric_router = std::make_unique<InMemoryMetricRouter>();
  auto meter_provider = opentelemetry::metrics::Provider::GetMeterProvider();
  auto meter = meter_provider->GetMeter("test_meter", "1", "dummy_schema_url");
  return meter->CreateUInt64Counter("test_counter", "test_counter_description");
}

// Benchmarking 3: Otel Counter increment.
void BM_OtelCounterIncrement(benchmark::State& state) {
  static auto counter = CreateOtelCounter();
  static const std::map<std::string, opentelemetry::common::AttributeValue>
      attributes_map = {{"attribute1", "value1"}, {"attribute2", 42}};

  for (auto _ : state) {
    counter->Add(10, attributes_map);
    benchmark::DoNotOptimize(counter);
  }
}

// Benchmarking 4: Atomic Counter Increment with OpenTelemetry Counter increment
// in background using AsyncExecutor.
void BM_OTelCounterAsyncIncrement(benchmark::State& state) {
  static auto async_executor = [] {
    auto executor = std::make_unique<AsyncExecutor>(
        std::thread::hardware_concurrency(), 1000);
    executor->Init();
    executor->Run();
    return executor;
  }();

  static auto otel_counter = CreateOtelCounter();
  static const std::map<std::string, opentelemetry::common::AttributeValue>
      attributes_map = {{"attribute1", "value1"}, {"attribute2", 42}};

  static std::atomic<int64_t> local_counter{0};

  for (auto _ : state) {
    // Increment the local counter.
    local_counter.fetch_add(1, std::memory_order_relaxed);

    // Schedule the async operation to update the counter.
    AsyncOperation operation = AsyncOperation([]() {
      int count = local_counter.load(std::memory_order_relaxed);
      otel_counter->Add(static_cast<uint64_t>(count), attributes_map);
    });

    async_executor->Schedule(operation, AsyncPriority::Normal);

    benchmark::DoNotOptimize(local_counter);
    benchmark::DoNotOptimize(otel_counter);
  }

  async_executor->Stop();
}

// Register the benchmarks.
BENCHMARK(BM_SpinLockIncrement)->ThreadRange(1, 64);
BENCHMARK(BM_AtomicIncrement)->ThreadRange(1, 64);
BENCHMARK(BM_OtelCounterIncrement)->ThreadRange(1, 64);
BENCHMARK(BM_OTelCounterAsyncIncrement)->ThreadRange(1, 64);

}  // namespace
}  // namespace privacy_sandbox::pbs_common

// Run the benchmark.
BENCHMARK_MAIN();
