package com.google.scp.coordinator.keymanagement.keystorage.tasks.gcp;

import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.SignDataKeyTask;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;
import com.google.scp.shared.api.exception.ServiceException;

/**
 * Stub SignDataKeyTask implementation.
 *
 * <p>Unusued because GCP's KeyStorageService does not use DataKeys at the moment.
 */
public final class GcpSignDataKeyTask implements SignDataKeyTask {
  @Override
  public DataKey signDataKey(DataKey dataKey) throws ServiceException {
    throw new IllegalStateException("Not implemented");
  }

  @Override
  public void verifyDataKey(DataKey dataKey) throws ServiceException {
    throw new IllegalStateException("Not implemented");
  }
}
