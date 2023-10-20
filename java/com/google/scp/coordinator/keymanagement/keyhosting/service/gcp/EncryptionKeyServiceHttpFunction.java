package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import com.google.inject.Guice;
import com.google.inject.Injector;

/** Handles requests to EncryptionKeyService and returns HTTP Response. */
public final class EncryptionKeyServiceHttpFunction extends EncryptionKeyServiceHttpFunctionBase {

  /**
   * Creates a new instance of the {@code EncryptionKeyServiceHttpFunction} class with the given
   * {@link GetEncryptionKeyRequestHandler} and {@link ListRecentEncryptionKeysRequestHandler}.
   */
  public EncryptionKeyServiceHttpFunction(
      GetEncryptionKeyRequestHandler getEncryptionKeyRequestHandler,
      ListRecentEncryptionKeysRequestHandler ListRecentEncryptionKeysRequestHandler) {
    super(getEncryptionKeyRequestHandler, ListRecentEncryptionKeysRequestHandler);
  }

  /** Creates a new instance of the {@code PrivateKeyServiceHttpFunction} class. */
  public EncryptionKeyServiceHttpFunction() {
    this(Guice.createInjector(new GcpKeyServiceModule()));
  }

  /**
   * Creates a new instance of the {@code PrivateKeyServiceHttpFunction} class with the given {@link
   * Injector}.
   */
  private EncryptionKeyServiceHttpFunction(Injector injector) {
    this(
        injector.getInstance(GetEncryptionKeyRequestHandler.class),
        injector.getInstance(ListRecentEncryptionKeysRequestHandler.class));
  }
}
