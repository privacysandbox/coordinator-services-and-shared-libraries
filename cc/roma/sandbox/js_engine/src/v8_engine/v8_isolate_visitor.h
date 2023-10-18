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

#include <vector>

#include "include/v8.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {
class V8IsolateVisitor {
 public:
  virtual core::ExecutionResult Visit(
      v8::Isolate* isolate,
      v8::Local<v8::ObjectTemplate>& global_object_template) noexcept = 0;

  virtual void AddExternalReferences(
      std::vector<intptr_t>& external_references) noexcept = 0;
};
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine
