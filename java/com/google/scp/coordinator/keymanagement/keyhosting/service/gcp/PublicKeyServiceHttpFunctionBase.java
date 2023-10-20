package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import static com.google.scp.shared.api.model.HttpMethod.GET;

import com.google.common.collect.ImmutableMap;
import com.google.scp.shared.api.model.HttpMethod;
import com.google.scp.shared.gcp.util.CloudFunctionRequestHandler;
import com.google.scp.shared.gcp.util.CloudFunctionServiceBase;
import java.util.regex.Pattern;

/** Base class for PublicKeyService. */
public abstract class PublicKeyServiceHttpFunctionBase extends CloudFunctionServiceBase {

  private static final String PATTERN_STR =
      String.format(
          "/.well-known/%s/v1/public-keys",
          System.getenv().getOrDefault("APPLICATION_NAME", "default"));
  private static final Pattern GET_PUBLIC_KEYS_URL_PATTERN =
      Pattern.compile(PATTERN_STR, Pattern.CASE_INSENSITIVE);
  private final GetPublicKeysRequestHandler getPublicKeysRequestHandler;

  /**
   * Creates a new instance of the {@code PublicKeyServiceHttpFunctionBase} class with the given
   * {@link GetPublicKeysRequestHandler}.
   */
  public PublicKeyServiceHttpFunctionBase(GetPublicKeysRequestHandler getPublicKeysRequestHandler) {
    this.getPublicKeysRequestHandler = getPublicKeysRequestHandler;
  }

  @Override
  protected ImmutableMap<HttpMethod, ImmutableMap<Pattern, CloudFunctionRequestHandler>>
      getRequestHandlerMap() {
    return ImmutableMap.of(
        GET, ImmutableMap.of(GET_PUBLIC_KEYS_URL_PATTERN, this.getPublicKeysRequestHandler));
  }
}
