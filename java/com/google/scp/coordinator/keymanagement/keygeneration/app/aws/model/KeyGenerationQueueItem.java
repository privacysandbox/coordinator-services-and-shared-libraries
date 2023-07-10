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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws.model;

import com.google.auto.value.AutoValue;
import java.time.Duration;

/**
 * Represents an item on the Key Generation queue. Because there is no payload, this type is
 * effectively only queue item metadata.
 */
@AutoValue
public abstract class KeyGenerationQueueItem {

  /** Returns a new instance of the builder for this class. */
  public static Builder builder() {
    return new AutoValue_KeyGenerationQueueItem.Builder();
  }

  /**
   * The processing time-out of the key generation message, as dictated by the queue implementation.
   */
  public abstract Duration processingTimeout();

  /**
   * The item receipt info that will be used to acknowledge processing of the item with the
   * KeyGenerationQueue.
   *
   * <p>At present all the KeyGenerationQueue implementations can use a string for this field, but
   * in the future it can be replaced by a more complex object or AutoOneOf if needed.
   */
  public abstract String receiptInfo();

  /** Builder class for the {@code KeyGenerationQueueItem} class. */
  @AutoValue.Builder
  public abstract static class Builder {

    /** Set the processing time-out of the key generation message. */
    public abstract Builder setProcessingTimeout(Duration processingTimeout);

    /**
     * Set the item receipt info that will be used to acknowledge processing of the item with the
     * KeyGenerationQueue.
     *
     * <p>At present all the KeyGenerationQueue implementations can use a string for this field, but
     * in the future it can be replaced by a more complex object or AutoOneOf if needed.
     */
    public abstract Builder setReceiptInfo(String receiptInfo);

    /** Returns a new instance of the {@code KeyGenerationQueueItem} class from the builder. */
    public abstract KeyGenerationQueueItem build();
  }
}
