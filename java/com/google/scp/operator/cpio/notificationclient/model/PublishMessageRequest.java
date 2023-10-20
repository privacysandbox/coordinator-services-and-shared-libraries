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

package com.google.scp.operator.cpio.notificationclient.model;

import com.fasterxml.jackson.annotation.JsonProperty;
import com.google.auto.value.AutoValue;
import com.google.common.collect.ImmutableMap;

/**
 * Representation of a single message to be published.
 *
 * <p>A message contains a body, a topic (destination), and a map of customizable attributes.
 */
@AutoValue
public abstract class PublishMessageRequest {

  /** Returns a new instance of the builder for this class. */
  public static Builder builder() {
    return new AutoValue_PublishMessageRequest.Builder()
      .setMessageAttributes(ImmutableMap.of());
  }

  /** Returns a new builder instance from a {@code PublishMessageRequest} instance. */
  public abstract Builder toBuilder();

  /** The body for the message. */
  @JsonProperty("message_body")
  public abstract String messageBody();

  /** The attributes for the message. */
  @JsonProperty("message_attributes")
  public abstract ImmutableMap<String, String> messageAttributes();

  /** The topic for the message. Uses Pub/Sub for GCP and SNS for AWS. */
  @JsonProperty("notification_topic")
  public abstract String notificationTopic();

  /** Builder for the {@code PublishMessageRequest} class. */
  @AutoValue.Builder
  public abstract static class Builder {

    /** Set the message body. */
    public abstract Builder setMessageBody(String messageBody);

    /** Set the message attributes. */
    public abstract Builder setMessageAttributes(ImmutableMap<String, String> messageAttributes);

    /** Set the message topic name (destination). */
    public abstract  Builder setNotificationTopic(String topicName);

    /** Creates a new instance of the {@code PublishMessageRequest} class from the builder. */
    public abstract PublishMessageRequest build();
  }
}
