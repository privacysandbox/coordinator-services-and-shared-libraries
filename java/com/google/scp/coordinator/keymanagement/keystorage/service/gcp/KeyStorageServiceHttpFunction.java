package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import com.google.inject.Guice;

/** Handles requests to KeyStorageService and returns HTTP Response. */
public final class KeyStorageServiceHttpFunction extends KeyStorageServiceHttpFunctionBase {
  /**
   * Creates a new instance of the {@code KeyStorageServiceHttpFunction} class with the given {@link
   * CreateKeyRequestHandler}.
   */
  public KeyStorageServiceHttpFunction(CreateKeyRequestHandler createKeyRequestHandler) {
    super(createKeyRequestHandler);
  }

  /** Creates a new instance of the {@code KeyStorageServiceHttpFunction} class. */
  public KeyStorageServiceHttpFunction() {
    this(
        Guice.createInjector(new GcpKeyStorageServiceModule())
            .getInstance(CreateKeyRequestHandler.class));
  }
}
