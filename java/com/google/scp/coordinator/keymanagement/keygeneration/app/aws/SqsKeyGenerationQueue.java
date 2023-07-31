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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws;

import static com.google.common.collect.MoreCollectors.toOptional;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationQueueMessageLeaseSeconds;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationSqsMaxWaitTimeSeconds;
import static com.google.scp.coordinator.keymanagement.keygeneration.app.aws.Annotations.KeyGenerationSqsUrl;

import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keygeneration.app.aws.model.KeyGenerationQueueItem;
import java.time.Duration;
import java.util.Optional;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import software.amazon.awssdk.core.exception.SdkException;
import software.amazon.awssdk.services.sqs.SqsClient;
import software.amazon.awssdk.services.sqs.model.DeleteMessageRequest;
import software.amazon.awssdk.services.sqs.model.ReceiveMessageRequest;

/** Functionality for accessing the key generation processing queue on AWS SQS */
public final class SqsKeyGenerationQueue {

  /**
   * SQS allows for batch receipt of messages, the worker should only receive single messages so
   * this is set to 1.
   */
  private static final int MAX_NUMBER_OF_MESSAGES_RECEIVED = 1;

  private static final Logger logger = LoggerFactory.getLogger(SqsKeyGenerationQueue.class);

  private final SqsClient sqsClient;
  private final String queueUrl;
  /**
   * Max time to wait to receive messages.
   *
   * @see
   *     https://docs.aws.amazon.com/AWSSimpleQueueService/latest/SQSDeveloperGuide/sqs-short-and-long-polling.html
   */
  private final int maxWaitTimeSeconds;
  /**
   * The "lease" length that the item receipt has. If the item is not acknowledged (deleted) within
   * this time window it will be visible on the queue again for another worker to pick up. See AWS
   * docs for more detail.
   *
   * @see
   *     https://docs.aws.amazon.com/AWSSimpleQueueService/latest/SQSDeveloperGuide/sqs-visibility-timeout.html
   */
  private final int visibilityTimeoutSeconds;

  /** Creates a new instance of the {@code SqsKeyGenerationQueue} class. */
  @Inject
  SqsKeyGenerationQueue(
      SqsClient sqsClient,
      @KeyGenerationSqsUrl String queueUrl,
      @KeyGenerationSqsMaxWaitTimeSeconds int maxWaitTimeSeconds,
      @KeyGenerationQueueMessageLeaseSeconds int visibilityTimeoutSeconds) {
    this.sqsClient = sqsClient;
    this.queueUrl = queueUrl;
    this.maxWaitTimeSeconds = maxWaitTimeSeconds;
    this.visibilityTimeoutSeconds = visibilityTimeoutSeconds;
  }

  /**
   * Blocking call to receive a key generation trigger.
   *
   * @return an {@code Optional} of a {@code KeyGenerationQueueItem}. The {@code
   *     KeyGenerationQueueItem} will be present if a message was received within the receipt
   *     timeout. It will be empty if no message was received.
   */
  public Optional<KeyGenerationQueueItem> pullQueueItem() throws SqsKeyGenerationQueueException {
    ReceiveMessageRequest receiveMessageRequest =
        ReceiveMessageRequest.builder()
            .queueUrl(queueUrl)
            .maxNumberOfMessages(MAX_NUMBER_OF_MESSAGES_RECEIVED)
            .waitTimeSeconds(maxWaitTimeSeconds)
            .visibilityTimeout(visibilityTimeoutSeconds)
            .build();

    try {
      Optional<KeyGenerationQueueItem> receivedKeyGeneration =
          sqsClient.receiveMessage(receiveMessageRequest).messages().stream()
              .collect(toOptional())
              .map(
                  message ->
                      KeyGenerationQueueItem.builder()
                          .setProcessingTimeout(Duration.ofSeconds(visibilityTimeoutSeconds))
                          .setReceiptInfo(message.receiptHandle())
                          .build());
      if (receivedKeyGeneration.isPresent()) {
        logger.info("Received key generation message from queue");
      } else {
        logger.info("No job received from queue");
      }
      return receivedKeyGeneration;
    } catch (SdkException e) {
      throw new SqsKeyGenerationQueueException("Error on awaiting message", e);
    }
  }

  /**
   * Acknowledge that a key generation was successfully processed so that it can be deleted from the
   * queue.
   *
   * @param keyGenerationQueueItem the item representing a key generation that was completed,
   *     containing receipt information for the queue.
   */
  public void acknowledgeKeyGenerationCompletion(KeyGenerationQueueItem keyGenerationQueueItem)
      throws SqsKeyGenerationQueueException {
    DeleteMessageRequest deleteMessageRequest =
        DeleteMessageRequest.builder()
            .queueUrl(queueUrl)
            .receiptHandle(keyGenerationQueueItem.receiptInfo())
            .build();

    try {
      sqsClient.deleteMessage(deleteMessageRequest);
      logger.info("Reporting processing completion for key generation message");
    } catch (SdkException e) {
      throw new SqsKeyGenerationQueueException("Error on acknowledgement of receipt", e);
    }
  }

  /** Represents an exception thrown by the {@code SqsKeyGenerationQueue} class. */
  class SqsKeyGenerationQueueException extends Exception {
    /** Creates a new instance of the {@code SqsKeyGenerationQueueException} class. */
    public SqsKeyGenerationQueueException(String message, Throwable cause) {
      super(message, cause);
    }
  }
}
