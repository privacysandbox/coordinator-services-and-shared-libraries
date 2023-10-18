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
#include <string>

#include "core/interface/service_interface.h"

namespace google::scp::cpio {
/// Struct for component creation function, component pointer and component
/// name.
struct ComponentCreator {
  virtual ~ComponentCreator() = default;

  ComponentCreator(
      std::function<
          core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>()>
          input_creation_func,
      const std::string& input_component_name)
      : creation_func(input_creation_func),
        component(nullptr),
        component_name(input_component_name) {}

  /// Component creation function.
  std::function<
      core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>()>
      creation_func;
  /// Component pointer. Will be assigned when it is created.
  std::shared_ptr<core::ServiceInterface> component;
  /// Component name.
  std::string component_name;
};

/// Interface for the factory to create components.
class ComponentFactoryInterface : public core::ServiceInterface {
 public:
  virtual ~ComponentFactoryInterface() = default;
};
}  // namespace google::scp::cpio
