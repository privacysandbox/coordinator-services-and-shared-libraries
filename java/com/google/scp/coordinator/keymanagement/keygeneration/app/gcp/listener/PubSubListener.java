package com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener;

import com.google.api.gax.core.CredentialsProvider;
import com.google.api.gax.core.NoCredentialsProvider;
import com.google.api.gax.grpc.GrpcTransportChannel;
import com.google.api.gax.rpc.FixedTransportChannelProvider;
import com.google.api.gax.rpc.TransportChannelProvider;
import com.google.cloud.pubsub.v1.AckReplyConsumer;
import com.google.cloud.pubsub.v1.MessageReceiver;
import com.google.cloud.pubsub.v1.Subscriber;
import com.google.pubsub.v1.ProjectSubscriptionName;
import com.google.pubsub.v1.PubsubMessage;
import com.google.scp.shared.api.exception.ServiceException;
import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Listens for pubsub message and generates keys when message is received. */
public abstract class PubSubListener {

  private static final Logger logger = LoggerFactory.getLogger(PubSubListener.class);

  private final String projectId;
  private final String subscriptionId;
  private final int numberOfKeysToCreate;
  private final int keysValidityInDays;
  private final int ttlInDays;

  private TransportChannelProvider channelProvider = null; // Only used for testing
  private CredentialsProvider credentialsProvider = null; // Only used for testing
  private int timeoutInSeconds = -1; // Only used for testing

  public PubSubListener(
      PubSubListenerConfig config,
      String projectId,
      String subscriptionId,
      int numberOfKeysToCreate,
      int keysValidityInDays,
      int ttlInDays) {
    this.projectId = projectId;
    this.subscriptionId = subscriptionId;
    this.numberOfKeysToCreate = numberOfKeysToCreate;
    this.keysValidityInDays = keysValidityInDays;
    this.ttlInDays = ttlInDays;
    config.endpointUrl().ifPresent(endpoint -> setTransportChannelAndCredentials(endpoint));
    config.timeoutInSeconds().ifPresent(timeout -> timeoutInSeconds = timeout);
  }

  /** Executes task that will generate keys (if needed) after pubsub message is received. */
  protected abstract void createKeys(
      int numberOfKeysToCreate, int keysValidityInDays, int ttlInDays) throws ServiceException;

  /** Sets up subscriber for given subscriptionId. Creates keys when message is received. */
  public void start() {
    ProjectSubscriptionName subscriptionName =
        ProjectSubscriptionName.of(projectId, subscriptionId);

    MessageReceiver receiver =
        getMessageReceiver(numberOfKeysToCreate, keysValidityInDays, ttlInDays);

    Subscriber subscriber = getSubscriber(subscriptionName, receiver);

    // Start the subscriber.
    subscriber.startAsync().awaitRunning();
    logger.info("Listening for messages on SubscriptionId: " + subscriptionId);

    if (timeoutInSeconds != -1) {
      try {
        subscriber.awaitTerminated(timeoutInSeconds, TimeUnit.SECONDS);
      } catch (TimeoutException e) {
        logger.info("TimeoutException occured.", e);
      }
    } else {
      subscriber.awaitTerminated();
    }
  }

  private Subscriber getSubscriber(
      ProjectSubscriptionName subscriptionName, MessageReceiver receiver) {
    Subscriber.Builder builder = Subscriber.newBuilder(subscriptionName, receiver);
    if (channelProvider != null) {
      builder.setChannelProvider(channelProvider);
    }
    if (credentialsProvider != null) {
      builder.setCredentialsProvider(credentialsProvider);
    }
    return builder.build();
  }

  private MessageReceiver getMessageReceiver(
      int numberOfKeysToCreate, int keysValidityInDays, int ttlInDays) {
    return (PubsubMessage message, AckReplyConsumer consumer) -> {
      // Handle incoming message, then ack the received message.
      logger.info(
          "Message Received. Id: "
              + message.getMessageId()
              + "Data: "
              + message.getData().toStringUtf8());
      try {
        createKeys(numberOfKeysToCreate, keysValidityInDays, ttlInDays);
        logger.info(
            "Task has been invoked. Send ack for message with Id: " + message.getMessageId());
        consumer.ack();
      } catch (ServiceException e) {
        logger.error("Error creating keys.", e);
      }
    };
  }

  private void setTransportChannelAndCredentials(String endpointUrl) {
    ManagedChannel channel = ManagedChannelBuilder.forTarget(endpointUrl).usePlaintext().build();
    this.channelProvider =
        FixedTransportChannelProvider.create(GrpcTransportChannel.create(channel));
    this.credentialsProvider = NoCredentialsProvider.create();
  }
}
