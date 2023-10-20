// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cc/core/dependency_injection/src/component_dependency_node.h"
#include "cc/core/dependency_injection/src/dependency_graph.h"
#include "public/core/interface/execution_result.h"

#include "service_interface.h"

namespace google::scp::core {
/**
 * @brief The interface for the DependencyInjectionService which provides basic
 * dependency injection functionality for components that implement
 * ServiceInterface.
 *
 */
class DependencyInjectionServiceInterface : public ServiceInterface {
 public:
  virtual ~DependencyInjectionServiceInterface() = default;

  /**
   * @brief Registers a component by id, by specifying its dependencies and
   * creation factory.
   *
   * @param id The id of the component.
   * @param dependencies The ids of the other components that this component
   * depends on.
   * @param factory The delegate factory used to create the component.
   * @return ExecutionResult
   */
  virtual ExecutionResult RegisterComponent(
      const std::string& id, const std::vector<std::string>& dependencies,
      std::function<std::shared_ptr<ServiceInterface>(
          const std::map<std::string, std::shared_ptr<ServiceInterface>>&)>
          factory) noexcept = 0;
  /**
   * @brief Creates all of the registered components.
   *
   * @return ExecutionResult
   */
  virtual ExecutionResult ResolveAll() noexcept = 0;
};
}  // namespace google::scp::core
