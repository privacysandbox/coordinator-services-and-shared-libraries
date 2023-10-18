/*
 * Copyright 2023 Google LLC
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
#include <vector>

#include "include/v8.h"
#include "public/core/interface/execution_result.h"
#include "roma/sandbox/js_engine/src/js_engine.h"
#include "roma/sandbox/worker/src/worker_utils.h"

#include "error_codes.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {
/// The type of the code content, including JavaScript,  WASM, and
/// JavaScript Mixed with WASM.
enum class CacheType { kUnknown, kSnapshot, kUnboundScript };

/**
 * @brief A snapshot of V8 isolate with a compilation context.
 *
 */
class SnapshotCompilationContext {
 public:
  v8::Isolate* v8_isolate{nullptr};

  CacheType cache_type;
  /// The startup data to hold the snapshot of the context which contains the
  /// compiled code.
  v8::StartupData startup_data{nullptr, 0};

  /// An instance of UnboundScript used to cache compiled code in isolate.
  v8::Global<v8::UnboundScript> unbound_script;

  ~SnapshotCompilationContext() {
    unbound_script.Reset();

    if (v8_isolate) {
      v8_isolate->Dispose();
      v8_isolate = nullptr;
    }

    // If there's any previous data, deallocate it.
    if (startup_data.data) {
      delete[] startup_data.data;
      startup_data = {nullptr, 0};
    }
  }
};
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine
