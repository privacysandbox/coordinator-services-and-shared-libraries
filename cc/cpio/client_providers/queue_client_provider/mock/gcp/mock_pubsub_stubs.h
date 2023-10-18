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

#include <gmock/gmock.h>

#include <memory>

#include <google/pubsub/v1/pubsub.grpc.pb.h>

namespace google::scp::cpio::client_providers::mock {

class MockPublisherStub : public google::pubsub::v1::Publisher::StubInterface {
 public:
  MOCK_METHOD(grpc::Status, CreateTopic,
              (grpc::ClientContext*, const google::pubsub::v1::Topic&,
               google::pubsub::v1::Topic*),
              (override));
  MOCK_METHOD(grpc::Status, UpdateTopic,
              (grpc::ClientContext*,
               const google::pubsub::v1::UpdateTopicRequest&,
               google::pubsub::v1::Topic*),
              (override));
  MOCK_METHOD(grpc::Status, Publish,
              (grpc::ClientContext*, const google::pubsub::v1::PublishRequest&,
               google::pubsub::v1::PublishResponse*),
              (override));
  MOCK_METHOD(grpc::Status, GetTopic,
              (grpc::ClientContext*,
               const ::google::pubsub::v1::GetTopicRequest&,
               google::pubsub::v1::Topic*),
              (override));
  MOCK_METHOD(grpc::Status, ListTopics,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListTopicsRequest&,
               google::pubsub::v1::ListTopicsResponse*),
              (override));
  MOCK_METHOD(grpc::Status, ListTopicSubscriptions,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListTopicSubscriptionsRequest&,
               google::pubsub::v1::ListTopicSubscriptionsResponse*),
              (override));
  MOCK_METHOD(grpc::Status, ListTopicSnapshots,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListTopicSnapshotsRequest&,
               google::pubsub::v1::ListTopicSnapshotsResponse*),
              (override));
  MOCK_METHOD(grpc::Status, DeleteTopic,
              (grpc::ClientContext*,
               const google::pubsub::v1::DeleteTopicRequest&,
               google::protobuf::Empty*),
              (override));
  MOCK_METHOD(grpc::Status, DetachSubscription,
              (grpc::ClientContext*,
               const google::pubsub::v1::DetachSubscriptionRequest&,
               google::pubsub::v1::DetachSubscriptionResponse*),
              (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::pubsub::v1::Topic>*,
      AsyncCreateTopicRaw,
      (grpc::ClientContext*, const google::pubsub::v1::Topic&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::pubsub::v1::Topic>*,
      PrepareAsyncCreateTopicRaw,
      (grpc::ClientContext*, const google::pubsub::v1::Topic&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::pubsub::v1::Topic>*,
      AsyncUpdateTopicRaw,
      (grpc::ClientContext*, const google::pubsub::v1::UpdateTopicRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::pubsub::v1::Topic>*,
      PrepareAsyncUpdateTopicRaw,
      (grpc::ClientContext*, const google::pubsub::v1::UpdateTopicRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::PublishResponse>*,
              AsyncPublishRaw,
              (grpc::ClientContext*, const google::pubsub::v1::PublishRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::PublishResponse>*,
              PrepareAsyncPublishRaw,
              (grpc::ClientContext*, const google::pubsub::v1::PublishRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::pubsub::v1::Topic>*,
      AsyncGetTopicRaw,
      (grpc::ClientContext*, const google::pubsub::v1::GetTopicRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::pubsub::v1::Topic>*,
      PrepareAsyncGetTopicRaw,
      (grpc::ClientContext*, const google::pubsub::v1::GetTopicRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::ListTopicsResponse>*,
              AsyncListTopicsRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListTopicsRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::ListTopicsResponse>*,
              PrepareAsyncListTopicsRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListTopicsRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::ListTopicSubscriptionsResponse>*,
              AsyncListTopicSubscriptionsRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListTopicSubscriptionsRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::ListTopicSubscriptionsResponse>*,
              PrepareAsyncListTopicSubscriptionsRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListTopicSubscriptionsRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::ListTopicSnapshotsResponse>*,
              AsyncListTopicSnapshotsRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListTopicSnapshotsRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::ListTopicSnapshotsResponse>*,
              PrepareAsyncListTopicSnapshotsRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListTopicSnapshotsRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*,
      AsyncDeleteTopicRaw,
      (grpc::ClientContext*, const google::pubsub::v1::DeleteTopicRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*,
      PrepareAsyncDeleteTopicRaw,
      (grpc::ClientContext*, const google::pubsub::v1::DeleteTopicRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::DetachSubscriptionResponse>*,
              AsyncDetachSubscriptionRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::DetachSubscriptionRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::DetachSubscriptionResponse>*,
              PrepareAsyncDetachSubscriptionRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::DetachSubscriptionRequest&,
               grpc::CompletionQueue*),
              (override));
};

class MockSubscriberStub
    : public google::pubsub::v1::Subscriber::StubInterface {
 public:
  MOCK_METHOD(grpc::Status, CreateSubscription,
              (grpc::ClientContext*, const google::pubsub::v1::Subscription&,
               google::pubsub::v1::Subscription*),
              (override));
  MOCK_METHOD(grpc::Status, GetSubscription,
              (grpc::ClientContext*,
               const google::pubsub::v1::GetSubscriptionRequest&,
               google::pubsub::v1::Subscription*),
              (override));
  MOCK_METHOD(grpc::Status, UpdateSubscription,
              (grpc::ClientContext*,
               const google::pubsub::v1::UpdateSubscriptionRequest&,
               google::pubsub::v1::Subscription*),
              (override));
  MOCK_METHOD(grpc::Status, ListSubscriptions,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListSubscriptionsRequest&,
               google::pubsub::v1::ListSubscriptionsResponse*),
              (override));
  MOCK_METHOD(grpc::Status, DeleteSubscription,
              (grpc::ClientContext*,
               const google::pubsub::v1::DeleteSubscriptionRequest&,
               google::protobuf::Empty*),
              (override));
  MOCK_METHOD(grpc::Status, ModifyAckDeadline,
              (grpc::ClientContext*,
               const google::pubsub::v1::ModifyAckDeadlineRequest&,
               google::protobuf::Empty*),
              (override));
  MOCK_METHOD(grpc::Status, Acknowledge,
              (grpc::ClientContext*,
               const google::pubsub::v1::AcknowledgeRequest&,
               google::protobuf::Empty*),
              (override));
  MOCK_METHOD(grpc::Status, Pull,
              (grpc::ClientContext*, const google::pubsub::v1::PullRequest&,
               google::pubsub::v1::PullResponse*),
              (override));
  MOCK_METHOD(grpc::Status, ModifyPushConfig,
              (grpc::ClientContext*,
               const google::pubsub::v1::ModifyPushConfigRequest&,
               google::protobuf::Empty*),
              (override));
  MOCK_METHOD(grpc::Status, GetSnapshot,
              (grpc::ClientContext*,
               const google::pubsub::v1::GetSnapshotRequest&,
               google::pubsub::v1::Snapshot*),
              (override));
  MOCK_METHOD(grpc::Status, ListSnapshots,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListSnapshotsRequest&,
               google::pubsub::v1::ListSnapshotsResponse*),
              (override));
  MOCK_METHOD(grpc::Status, CreateSnapshot,
              (grpc::ClientContext*,
               const google::pubsub::v1::CreateSnapshotRequest&,
               google::pubsub::v1::Snapshot*),
              (override));
  MOCK_METHOD(grpc::Status, UpdateSnapshot,
              (grpc::ClientContext*,
               const google::pubsub::v1::UpdateSnapshotRequest&,
               google::pubsub::v1::Snapshot*),
              (override));
  MOCK_METHOD(grpc::Status, DeleteSnapshot,
              (grpc::ClientContext*,
               const google::pubsub::v1::DeleteSnapshotRequest&,
               google::protobuf::Empty*),
              (override));
  MOCK_METHOD(grpc::Status, Seek,
              (grpc::ClientContext*, const google::pubsub::v1::SeekRequest&,
               google::pubsub::v1::SeekResponse*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::Subscription>*,
              AsyncCreateSubscriptionRaw,
              (grpc::ClientContext*, const google::pubsub::v1::Subscription&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::Subscription>*,
              PrepareAsyncCreateSubscriptionRaw,
              (grpc::ClientContext*, const google::pubsub::v1::Subscription&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::Subscription>*,
              AsyncGetSubscriptionRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::GetSubscriptionRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::Subscription>*,
              PrepareAsyncGetSubscriptionRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::GetSubscriptionRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::Subscription>*,
              AsyncUpdateSubscriptionRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::UpdateSubscriptionRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::Subscription>*,
              PrepareAsyncUpdateSubscriptionRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::UpdateSubscriptionRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::ListSubscriptionsResponse>*,
              AsyncListSubscriptionsRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListSubscriptionsRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::ListSubscriptionsResponse>*,
              PrepareAsyncListSubscriptionsRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListSubscriptionsRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*,
      AsyncDeleteSubscriptionRaw,
      (grpc::ClientContext*,
       const google::pubsub::v1::DeleteSubscriptionRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*,
      PrepareAsyncDeleteSubscriptionRaw,
      (grpc::ClientContext*,
       const google::pubsub::v1::DeleteSubscriptionRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*,
      AsyncModifyAckDeadlineRaw,
      (grpc::ClientContext*,
       const google::pubsub::v1::ModifyAckDeadlineRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*,
      PrepareAsyncModifyAckDeadlineRaw,
      (grpc::ClientContext*,
       const google::pubsub::v1::ModifyAckDeadlineRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*,
      AsyncAcknowledgeRaw,
      (grpc::ClientContext*, const google::pubsub::v1::AcknowledgeRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*,
      PrepareAsyncAcknowledgeRaw,
      (grpc::ClientContext*, const google::pubsub::v1::AcknowledgeRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::PullResponse>*,
              AsyncPullRaw,
              (grpc::ClientContext*, const google::pubsub::v1::PullRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::PullResponse>*,
              PrepareAsyncPullRaw,
              (grpc::ClientContext*, const google::pubsub::v1::PullRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD((grpc::ClientReaderWriterInterface<
                  google::pubsub::v1::StreamingPullRequest,
                  google::pubsub::v1::StreamingPullResponse>*),
              StreamingPullRaw, (grpc::ClientContext*), (override));
  MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<
                  google::pubsub::v1::StreamingPullRequest,
                  google::pubsub::v1::StreamingPullResponse>*),
              AsyncStreamingPullRaw,
              (grpc::ClientContext*, grpc::CompletionQueue*, void*),
              (override));
  MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<
                  google::pubsub::v1::StreamingPullRequest,
                  google::pubsub::v1::StreamingPullResponse>*),
              PrepareAsyncStreamingPullRaw,
              (grpc::ClientContext*, grpc::CompletionQueue*), (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*,
      AsyncModifyPushConfigRaw,
      (grpc::ClientContext*, const google::pubsub::v1::ModifyPushConfigRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*,
      PrepareAsyncModifyPushConfigRaw,
      (grpc::ClientContext*, const google::pubsub::v1::ModifyPushConfigRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::pubsub::v1::Snapshot>*,
      AsyncGetSnapshotRaw,
      (grpc::ClientContext*, const google::pubsub::v1::GetSnapshotRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::pubsub::v1::Snapshot>*,
      PrepareAsyncGetSnapshotRaw,
      (grpc::ClientContext*, const google::pubsub::v1::GetSnapshotRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::ListSnapshotsResponse>*,
              AsyncListSnapshotsRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListSnapshotsRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::ListSnapshotsResponse>*,
              PrepareAsyncListSnapshotsRaw,
              (grpc::ClientContext*,
               const google::pubsub::v1::ListSnapshotsRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::pubsub::v1::Snapshot>*,
      AsyncCreateSnapshotRaw,
      (grpc::ClientContext*, const google::pubsub::v1::CreateSnapshotRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::pubsub::v1::Snapshot>*,
      PrepareAsyncCreateSnapshotRaw,
      (grpc::ClientContext*, const google::pubsub::v1::CreateSnapshotRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::pubsub::v1::Snapshot>*,
      AsyncUpdateSnapshotRaw,
      (grpc::ClientContext*, const google::pubsub::v1::UpdateSnapshotRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::pubsub::v1::Snapshot>*,
      PrepareAsyncUpdateSnapshotRaw,
      (grpc::ClientContext*, const google::pubsub::v1::UpdateSnapshotRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*,
      AsyncDeleteSnapshotRaw,
      (grpc::ClientContext*, const google::pubsub::v1::DeleteSnapshotRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>*,
      PrepareAsyncDeleteSnapshotRaw,
      (grpc::ClientContext*, const google::pubsub::v1::DeleteSnapshotRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::SeekResponse>*,
              AsyncSeekRaw,
              (grpc::ClientContext*, const google::pubsub::v1::SeekRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<
                  google::pubsub::v1::SeekResponse>*,
              PrepareAsyncSeekRaw,
              (grpc::ClientContext*, const google::pubsub::v1::SeekRequest&,
               grpc::CompletionQueue*),
              (override));
};

}  // namespace google::scp::cpio::client_providers::mock
