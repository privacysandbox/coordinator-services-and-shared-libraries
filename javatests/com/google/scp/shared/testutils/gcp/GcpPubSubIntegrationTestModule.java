package com.google.scp.shared.testutils.gcp;

import static com.google.scp.shared.gcp.Constants.GCP_TEST_PROJECT_ID;

import com.google.api.gax.core.CredentialsProvider;
import com.google.api.gax.core.NoCredentialsProvider;
import com.google.api.gax.rpc.TransportChannelProvider;
import com.google.cloud.pubsub.v1.Publisher;
import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.shared.testutils.gcp.Annotations.JobSubscriptionId;
import com.google.scp.shared.testutils.gcp.Annotations.JobTopicId;
import com.google.scp.shared.testutils.gcp.Annotations.KeyGenerationPublisher;
import com.google.scp.shared.testutils.gcp.Annotations.KeyGenerationSubscriptionId;
import com.google.scp.shared.testutils.gcp.Annotations.KeyGenerationTopicId;
import com.google.scp.shared.testutils.gcp.Annotations.PubSubProjectId;
import java.io.IOException;

/**
 * This module creates a pubsub emulator along with the 2 topics/subscriptions. One for the key
 * generation and one for the job queue.
 */
public class GcpPubSubIntegrationTestModule extends AbstractModule {

  private static final String KEY_GEN_TOPIC_ID = "keyGenTopic";
  private static final String KEY_GEN_SUBSCRIPTION_ID = "keyGenSub";

  private static final String JOB_TOPIC_ID = "jobTopic";
  private static final String JOB_SUBSCRIPTION_ID = "jobSub";

  @Override
  public void configure() {
    // This will create the first set of publisher/subscriber for the job topic.
    install(
        new PubSubEmulatorContainerTestModule(
            GCP_TEST_PROJECT_ID, JOB_TOPIC_ID, JOB_SUBSCRIPTION_ID, true));
  }

  @Provides
  @Singleton
  @KeyGenerationPublisher
  public Publisher providesKeyGenerationPublisher(
      TransportChannelProvider transportChannelProvider) {
    CredentialsProvider credentialsProvider = NoCredentialsProvider.create();
    try {
      PubSubContainerUtil.createTopic(
          GCP_TEST_PROJECT_ID, KEY_GEN_TOPIC_ID, transportChannelProvider, credentialsProvider);
      PubSubContainerUtil.createSubscription(
          GCP_TEST_PROJECT_ID,
          KEY_GEN_TOPIC_ID,
          KEY_GEN_SUBSCRIPTION_ID,
          transportChannelProvider,
          credentialsProvider);
      return PubSubContainerUtil.createPublisher(
          GCP_TEST_PROJECT_ID, KEY_GEN_TOPIC_ID, transportChannelProvider, credentialsProvider);
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  @Provides
  @PubSubProjectId
  public String providesPubSubProjectId() {
    return GCP_TEST_PROJECT_ID;
  }

  @Provides
  @JobTopicId
  public String providesJobTopicId() {
    return JOB_TOPIC_ID;
  }

  @Provides
  @JobSubscriptionId
  public String providesJobSubscriptionId() {
    return JOB_SUBSCRIPTION_ID;
  }

  @Provides
  @KeyGenerationTopicId
  public String providesKeyGenerationTopicId() {
    return KEY_GEN_TOPIC_ID;
  }

  @Provides
  @KeyGenerationSubscriptionId
  public String providesKeyGenerationSubscriptionId() {
    return KEY_GEN_SUBSCRIPTION_ID;
  }
}
