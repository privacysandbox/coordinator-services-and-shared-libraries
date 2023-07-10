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

package com.google.scp.operator.cpio.cryptoclient.testing;

import com.google.scp.operator.cpio.cryptoclient.PrivateKeyFetchingService;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;

/** Fake PrivateKeyFetchingService which returns a preconfigured response. */
public final class FakePrivateKeyFetchingService implements PrivateKeyFetchingService {

  private String response = "";
  private boolean throwException = false;
  private Code errorCause = Code.UNKNOWN;
  private String errorMessage = "";

  /**
   * Returns the response set by the {@code setResponse} method, or an empty String if it was not
   * set.
   */
  public String fetchKeyCiphertext(String keyId) throws PrivateKeyFetchingServiceException {
    if (throwException) {
      throwException = false;
      throw new PrivateKeyFetchingServiceException(
          new ServiceException(errorCause, "Test Exception", errorMessage));
    }
    return response;
  }

  /** Set a response String to be returned by the {@code fetchKeyCiphertext} method. */
  public void setResponse(String newResponse) {
    response = newResponse;
  }

  /**
   * Set this instance to throw a ServiceException the next time fetchKeyCiphertext is called.
   *
   * @param code Code to set the ErrorCause in the thrown ServiceException to
   */
  public void setExceptionContents(Code code, String message) {
    throwException = true;
    errorCause = code;
    errorMessage = message;
  }
}
