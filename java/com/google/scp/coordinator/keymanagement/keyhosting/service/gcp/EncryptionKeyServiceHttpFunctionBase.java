package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import static com.google.scp.shared.api.model.HttpMethod.GET;

import com.google.common.collect.ImmutableMap;
import com.google.scp.shared.api.model.HttpMethod;
import com.google.scp.shared.gcp.util.CloudFunctionRequestHandler;
import com.google.scp.shared.gcp.util.CloudFunctionServiceBase;
import java.util.regex.Pattern;

/** Base class for EncryptionKeyService. */
public abstract class EncryptionKeyServiceHttpFunctionBase extends CloudFunctionServiceBase {

  // Example: /v1/encryptionKeys/abc-123
  private static final Pattern GET_ENCRYPTION_KEY_URL_PATTERN =
      Pattern.compile("/v1alpha/encryptionKeys/[^/]*", Pattern.CASE_INSENSITIVE);
  private final GetEncryptionKeyRequestHandler getEncryptionKeyRequestHandler;

  private static final Pattern LIST_RECENT_KEYS_URL_PATTERN =
      Pattern.compile("/v1alpha/encryptionKeys:recent", Pattern.CASE_INSENSITIVE);
  private final ListRecentEncryptionKeysRequestHandler listRecentKeysRequestHandler;

  /**
   * Creates a new instance of the {@code EncryptionKeyServiceHttpFunctionBase} class with the given
   * {@link GetEncryptionKeyRequestHandler} and {@link ListRecentEncryptionKeysRequestHandler}.
   */
  public EncryptionKeyServiceHttpFunctionBase(
      GetEncryptionKeyRequestHandler getEncryptionKeyRequestHandler,
      ListRecentEncryptionKeysRequestHandler listRecentKeysRequestHandler) {
    this.getEncryptionKeyRequestHandler = getEncryptionKeyRequestHandler;
    this.listRecentKeysRequestHandler = listRecentKeysRequestHandler;
  }

  @Override
  protected ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
      getRequestHandlerMap() {
    return ImmutableMap.of(
        GET,
        ImmutableMap.of(
            GET_ENCRYPTION_KEY_URL_PATTERN,
            this.getEncryptionKeyRequestHandler,
            LIST_RECENT_KEYS_URL_PATTERN,
            this.listRecentKeysRequestHandler));
  }
}
