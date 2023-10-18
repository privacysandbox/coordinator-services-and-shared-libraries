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

#include <memory>
#include <string>

#include <google/pubsub/v1/pubsub.grpc.pb.h>

namespace google::scp::core::test {
/**
 * @brief Create a Publisher Stub object.
 *
 * @param endpoint the endpoint to the Publisher
 * @return std::unique_ptr<pubsub::v1::Publisher::StubInterface> the returned
 * Publisher stub.
 */
std::unique_ptr<pubsub::v1::Publisher::StubInterface> CreatePublisherStub(
    const std::string& endpoint);

/**
 * @brief Create a Topic object in the given Publisher.
 *
 * @param stub the given Publisher.
 * @param project_id the project ID for the topic.
 * @param topic_id the topic ID to be created.
 */
void CreateTopic(pubsub::v1::Publisher::StubInterface& stub,
                 const std::string& project_id, const std::string& topic_id);
}  // namespace google::scp::core::test
