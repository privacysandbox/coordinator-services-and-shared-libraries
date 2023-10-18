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

#include <benchmark/benchmark.h>

#include "cc/roma/benchmark/src/roma_benchmark.h"

using google::scp::roma::benchmark::InputsType;
using google::scp::roma::benchmark::RomaBenchmarkSuite;
using google::scp::roma::benchmark::TestConfiguration;

static void RomaPayloadTest(benchmark::State& state) {
  for (auto _ : state) {
    TestConfiguration test_configuration;
    test_configuration.workers = 1;
    test_configuration.inputs_type = InputsType::kSimpleString;
    test_configuration.input_payload_in_byte = state.range(0);
    test_configuration.queue_size = 100;
    test_configuration.batch_size = 1;
    test_configuration.request_threads = 1;
    test_configuration.requests_per_thread = 10000;

    RomaBenchmarkSuite(test_configuration);
  }
}

static void RomaJsonInputParsingTest(benchmark::State& state) {
  for (auto _ : state) {
    TestConfiguration test_configuration;
    test_configuration.workers = 1;
    test_configuration.inputs_type = InputsType::kNestedJsonString;
    test_configuration.input_json_nested_depth = state.range(0);
    test_configuration.queue_size = 100;
    test_configuration.batch_size = 1;
    test_configuration.request_threads = 1;
    test_configuration.requests_per_thread = 10000;

    RomaBenchmarkSuite(test_configuration);
  }
}

static void RomaWorkerAndQueueTest(benchmark::State& state) {
  for (auto _ : state) {
    TestConfiguration test_configuration;
    test_configuration.workers = state.range(0);
    test_configuration.inputs_type = InputsType::kSimpleString;
    test_configuration.input_payload_in_byte = 500000;
    test_configuration.queue_size = state.range(1);
    test_configuration.batch_size = 1;
    test_configuration.request_threads = 1;
    test_configuration.requests_per_thread = 10000;

    RomaBenchmarkSuite(test_configuration);
  }
}

static void CustomArgumentsForWorkerAndQueueTest(
    benchmark::internal::Benchmark* b) {
  // worker size: {1, 4, 16}
  // queue size: {1, 10, 100, 1000}
  for (int i = 1; i <= 16; i *= 4) {
    for (int j = 1; j <= 1000; j *= 10) {
      b->Args({i, j});
    }
  }
}

static void RomaBatchSizeTest(benchmark::State& state) {
  for (auto _ : state) {
    TestConfiguration test_configuration;
    test_configuration.workers = 16;
    test_configuration.inputs_type = InputsType::kSimpleString;
    test_configuration.input_payload_in_byte = 500000;
    test_configuration.queue_size = 10;
    test_configuration.batch_size = state.range(0);
    test_configuration.request_threads = 1;
    test_configuration.requests_per_thread = 10000;

    RomaBenchmarkSuite(test_configuration);
  }
}

// Register the function as a benchmark
BENCHMARK(RomaJsonInputParsingTest)->RangeMultiplier(10)->Range(1, 100000);
BENCHMARK(RomaPayloadTest)->DenseRange(0, 1024 * 1024, 1024 * 500);
BENCHMARK(RomaWorkerAndQueueTest)->Apply(CustomArgumentsForWorkerAndQueueTest);
BENCHMARK(RomaBatchSizeTest)->RangeMultiplier(10)->Range(1, 100);

// Run the benchmark
BENCHMARK_MAIN();
