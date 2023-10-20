package com.google.scp.coordinator.keymanagement.keyhosting.service.gcp;

import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.api.model.HttpMethod.GET;
import static com.google.scp.shared.api.util.RequestUtil.getVariableFromPath;
import static com.google.scp.shared.api.util.RequestUtil.validateHttpMethod;
import static com.google.scp.shared.gcp.util.CloudFunctionUtil.createCloudFunctionResponseFromProto;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.common.collect.ImmutableMap;
import com.google.inject.Inject;
import com.google.scp.coordinator.keymanagement.keyhosting.service.common.KeyService;
import com.google.scp.coordinator.protos.keymanagement.keyhosting.api.v1.GetEncryptionKeyRequestProto.GetEncryptionKeyRequest;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.gcp.util.CloudFunctionRequestHandlerBase;
import java.io.IOException;

/** Request handler for fetching a specific encrypted key. */
public class GetEncryptionKeyRequestHandler
    extends CloudFunctionRequestHandlerBase<GetEncryptionKeyRequest, EncryptionKey> {
  private static final String ENCRYPTION_KEY_RESOURCE = "encryptionKeys/";

  /** Path template for getting keyId path variable when invoked by HttpRequest.getPath(). */
  private static final String PATH_TEMPLATE = "/v1alpha/encryptionKeys/:keyId";

  private final KeyService keyService;

  /** Creates a new instance of the {@code GetPrivateKeyRequestHandler} class. */
  @Inject
  public GetEncryptionKeyRequestHandler(KeyService keyService) {
    this.keyService = keyService;
  }

  @Override
  protected GetEncryptionKeyRequest toRequest(HttpRequest httpRequest) throws ServiceException {
    validateHttpMethod(httpRequest.getMethod(), GET);
    return GetEncryptionKeyRequest.newBuilder()
        .setName(
            ENCRYPTION_KEY_RESOURCE
                + getVariableFromPath(
                    /* pathTemplate = */ PATH_TEMPLATE,
                    /* path = */ httpRequest.getPath(),
                    /* variableName = */ "keyId"))
        .build();
  }

  @Override
  protected EncryptionKey processRequest(GetEncryptionKeyRequest getEncryptionKeyRequest)
      throws ServiceException {
    return keyService.getEncryptionKey(getEncryptionKeyRequest);
  }

  @Override
  protected void toCloudFunctionResponse(HttpResponse httpResponse, EncryptionKey response)
      throws IOException {
    createCloudFunctionResponseFromProto(
        httpResponse, response, OK.getHttpStatusCode(), ImmutableMap.of());
  }
}
