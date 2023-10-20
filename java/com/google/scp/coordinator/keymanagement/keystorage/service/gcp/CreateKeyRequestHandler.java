package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.Code.OK;
import static com.google.scp.shared.api.model.HttpMethod.POST;
import static com.google.scp.shared.api.util.RequestUtil.validateHttpMethod;
import static com.google.scp.shared.gcp.util.CloudFunctionUtil.createCloudFunctionResponseFromProto;

import com.google.cloud.functions.HttpRequest;
import com.google.cloud.functions.HttpResponse;
import com.google.common.collect.ImmutableMap;
import com.google.inject.Inject;
import com.google.protobuf.util.JsonFormat;
import com.google.scp.coordinator.keymanagement.keystorage.service.common.KeyStorageService;
import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.CreateKeyRequestProto.CreateKeyRequest;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.gcp.util.CloudFunctionRequestHandlerBase;
import java.io.IOException;

/** GCP Cloud function for handling create key requests. */
public class CreateKeyRequestHandler
    extends CloudFunctionRequestHandlerBase<CreateKeyRequest, EncryptionKey> {
  private final KeyStorageService keyStorageService;

  @Inject
  public CreateKeyRequestHandler(KeyStorageService keyStorageService) {
    this.keyStorageService = keyStorageService;
  }

  @Override
  protected CreateKeyRequest toRequest(HttpRequest httpRequest) throws ServiceException {
    validateHttpMethod(httpRequest.getMethod(), POST);
    try {
      CreateKeyRequest.Builder builder = CreateKeyRequest.newBuilder();
      JsonFormat.parser().merge(httpRequest.getReader(), builder);
      return builder.build();
    } catch (IOException e) {
      throw new ServiceException(
          INVALID_ARGUMENT, INVALID_ARGUMENT.name(), "Failed to parse CreateKeyRequest");
    }
  }

  @Override
  protected EncryptionKey processRequest(CreateKeyRequest createKeyRequest)
      throws ServiceException {
    return keyStorageService.createKey(createKeyRequest);
  }

  @Override
  protected void toCloudFunctionResponse(HttpResponse httpResponse, EncryptionKey key)
      throws IOException {
    createCloudFunctionResponseFromProto(
        httpResponse, key, OK.getHttpStatusCode(), ImmutableMap.of());
  }
}
