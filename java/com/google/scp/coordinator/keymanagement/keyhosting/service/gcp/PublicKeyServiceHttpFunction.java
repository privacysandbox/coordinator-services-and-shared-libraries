package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import com.google.inject.Guice;
import com.google.inject.Injector;
import io.opentelemetry.api.OpenTelemetry;

/** Handles requests to PublicKeyService and returns HTTP Response. */
public final class PublicKeyServiceHttpFunction extends PublicKeyServiceHttpFunctionBase {

  /**
   * Creates a new instance of the {@code PublicKeyServiceHttpFunction} class with the given {@link
   * GetPublicKeysRequestHandler}.
   */
  public PublicKeyServiceHttpFunction(
      GetPublicKeysRequestHandler getPublicKeysRequestHandler, OpenTelemetry OTel) {
    super(getPublicKeysRequestHandler, OTel);
  }

  /** Creates a new instance of the {@code PrivateKeyServiceHttpFunction} class. */
  public PublicKeyServiceHttpFunction() {
    this(Guice.createInjector(new GcpKeyServiceModule()));
  }

  /** Creates a new instance of the {@code PublicKeyServiceHttpFunction} class. */
  public PublicKeyServiceHttpFunction(Injector injector) {
    this(
        injector.getInstance(GetPublicKeysRequestHandler.class),
        injector.getInstance(OpenTelemetry.class));
  }
}
