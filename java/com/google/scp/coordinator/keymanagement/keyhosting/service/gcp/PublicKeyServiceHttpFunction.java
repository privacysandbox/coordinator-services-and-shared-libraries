package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import com.google.inject.Guice;

/** Handles requests to PublicKeyService and returns HTTP Response. */
public final class PublicKeyServiceHttpFunction extends PublicKeyServiceHttpFunctionBase {

  /**
   * Creates a new instance of the {@code PublicKeyServiceHttpFunction} class with the given {@link
   * GetPublicKeysRequestHandler}.
   */
  public PublicKeyServiceHttpFunction(GetPublicKeysRequestHandler getPublicKeysRequestHandler) {
    super(getPublicKeysRequestHandler);
  }

  /** Creates a new instance of the {@code PublicKeyServiceHttpFunction} class. */
  public PublicKeyServiceHttpFunction() {
    this(
        Guice.createInjector(new GcpKeyServiceModule())
            .getInstance(GetPublicKeysRequestHandler.class));
  }
}
