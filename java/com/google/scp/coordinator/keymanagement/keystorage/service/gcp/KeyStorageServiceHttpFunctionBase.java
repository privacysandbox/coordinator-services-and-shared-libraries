package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import static com.google.scp.shared.api.model.HttpMethod.POST;

import com.google.common.collect.ImmutableMap;
import com.google.scp.shared.api.model.HttpMethod;
import com.google.scp.shared.gcp.util.CloudFunctionRequestHandler;
import com.google.scp.shared.gcp.util.CloudFunctionServiceBase;
import java.util.regex.Pattern;

/** Base class for KeyStorageService. */
public abstract class KeyStorageServiceHttpFunctionBase extends CloudFunctionServiceBase {

  private static final Pattern CREATE_KEY_URL_PATTERN =
      Pattern.compile("/v1alpha/encryptionKeys", Pattern.CASE_INSENSITIVE);
  private final CreateKeyRequestHandler createKeyRequestHandler;

  /**
   * Creates a new instance of the {@code KeyStorageServiceHttpFunctionBase} class with the given
   * {@link CreateKeyRequestHandler}.
   */
  public KeyStorageServiceHttpFunctionBase(CreateKeyRequestHandler createKeyRequestHandler) {
    this.createKeyRequestHandler = createKeyRequestHandler;
  }

  @Override
  protected ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
      getRequestHandlerMap() {
    return ImmutableMap.of(
        POST, ImmutableMap.of(CREATE_KEY_URL_PATTERN, this.createKeyRequestHandler));
  }
}
