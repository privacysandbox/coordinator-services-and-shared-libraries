package com.google.scp.coordinator.keymanagement.keygeneration.app.gcp;

import com.beust.jcommander.JCommander;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener.PubSubListener;

/**
 * Application which subscribes to generateKeys event and generates keys when it receives the event.
 */
public final class KeyGenerationApplication {

  /** Parses input arguments and starts listening for key generation event. */
  public static void main(String[] args) {
    KeyGenerationArgs params = new KeyGenerationArgs();
    JCommander.newBuilder().addObject(params).build().parse(args);

    Injector injector = Guice.createInjector(new KeyGenerationModule(params));
    PubSubListener listener = injector.getInstance(PubSubListener.class);
    listener.start();
  }
}
