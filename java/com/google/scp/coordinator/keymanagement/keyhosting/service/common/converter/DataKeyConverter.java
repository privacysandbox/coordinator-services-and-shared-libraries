/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.coordinator.keymanagement.keyhosting.service.common.converter;

import com.google.common.base.Converter;
import com.google.scp.coordinator.protos.keymanagement.keystorage.api.v1.DataKeyProto;
import com.google.scp.coordinator.protos.keymanagement.shared.backend.DataKeyProto.DataKey;

/** Converts a DataKey between its service model and backend model. */
public final class DataKeyConverter extends Converter<DataKey, DataKeyProto.DataKey> {

  public static final DataKeyConverter INSTANCE = new DataKeyConverter();

  @Override
  protected DataKeyProto.DataKey doForward(DataKey backendModel) {
    return DataKeyProto.DataKey.newBuilder()
        .setEncryptedDataKey(backendModel.getEncryptedDataKey())
        .setEncryptedDataKeyKekUri(backendModel.getEncryptedDataKeyKekUri())
        .setDataKeyContext(backendModel.getDataKeyContext())
        .setMessageAuthenticationCode(backendModel.getMessageAuthenticationCode())
        .build();
  }

  @Override
  protected DataKey doBackward(DataKeyProto.DataKey serviceModel) {
    return DataKey.newBuilder()
        .setEncryptedDataKey(serviceModel.getEncryptedDataKey())
        .setEncryptedDataKeyKekUri(serviceModel.getEncryptedDataKeyKekUri())
        .setDataKeyContext(serviceModel.getDataKeyContext())
        .setMessageAuthenticationCode(serviceModel.getMessageAuthenticationCode())
        .build();
  }
}
