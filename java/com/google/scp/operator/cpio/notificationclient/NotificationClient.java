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

package com.google.scp.operator.cpio.notificationclient;

import com.google.scp.operator.cpio.notificationclient.model.PublishMessageRequest;

/** Interface for notification services. */
public interface NotificationClient {

  /**
   * Publishes a message to a cloud service (Pub/Sub or SNS)
   *
   * @param {@code PublishMessageRequest} object representing a message, the message desination, and
   *     any message attributes.
   */
  void publishMessage(PublishMessageRequest publishMessageRequest)
      throws NotificationClientException;

  /** Represents an exception thrown by the {@code NotificationClient} class. */
  class NotificationClientException extends Exception {
    /** Creates a new instance from a {@code Throwable}. */
    public NotificationClientException(Throwable cause) {
      super(cause);
    }
  }
}
