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

package com.google.scp.shared.api.util;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_HTTP_METHOD;
import static com.google.scp.shared.api.exception.SharedErrorReason.INVALID_URL_PATH_OR_VARIABLE;
import static com.google.scp.shared.api.model.Code.INVALID_ARGUMENT;
import static com.google.scp.shared.api.model.HttpMethod.GET;
import static com.google.scp.shared.api.model.HttpMethod.POST;
import static com.google.scp.shared.api.util.RequestUtil.getVariableFromPath;
import static com.google.scp.shared.api.util.RequestUtil.validateHttpMethod;
import static java.util.UUID.randomUUID;
import static org.junit.Assert.assertThrows;

import com.google.scp.shared.api.exception.ServiceException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class RequestUtilTest {

  @Test
  public void getVariableFromPath_getsVariableFromPath() throws ServiceException {
    String variable = randomUUID().toString();
    String variableName = "test";
    String template = "/test/:test";
    String pathToTest = "/test/" + variable;

    String varFromPath = getVariableFromPath(template, pathToTest, variableName);

    assertThat(varFromPath).isEqualTo(variable);
  }

  @Test
  public void getVariableFromPath_getsVariableFromPathWithMultipleVariables()
      throws ServiceException {
    String variable = randomUUID().toString();
    String variableName = "book";
    String template = "/shelf/:shelf/book/:book";
    String pathToTest = "/shelf/:shelf/book/" + variable;

    String varFromPath = getVariableFromPath(template, pathToTest, variableName);

    assertThat(varFromPath).isEqualTo(variable);
  }

  @Test
  public void getVariableFromPath_variableNotFound() {
    String variableName = "not_found";
    String template = "/testResource/test";
    String pathToTest = "/testResource/test";

    ServiceException exception =
        assertThrows(
            ServiceException.class, () -> getVariableFromPath(template, pathToTest, variableName));

    assertThat(exception.getErrorCode()).isEqualTo(INVALID_ARGUMENT);
    assertThat(exception.getErrorReason()).isEqualTo(INVALID_URL_PATH_OR_VARIABLE.name());
    assertThat(exception.getMessage()).isEqualTo("Variable not found in path: 'not_found'");
  }

  @Test
  public void getVariableFromPath_wrongVariable() {
    String variable = randomUUID().toString();
    String variableName = "not_found";
    String template = "/testResource/:test";
    String pathToTest = "/testResource/" + variable;

    ServiceException exception =
        assertThrows(
            ServiceException.class, () -> getVariableFromPath(template, pathToTest, variableName));

    assertThat(exception.getErrorCode()).isEqualTo(INVALID_ARGUMENT);
    assertThat(exception.getErrorReason()).isEqualTo(INVALID_URL_PATH_OR_VARIABLE.name());
    assertThat(exception.getMessage()).isEqualTo("Variable not found in path: 'not_found'");
  }

  @Test
  public void getVariableFromPath_invalidPathResource() {
    String variable = randomUUID().toString();
    String variableName = "test";
    String template = "/testResource/:test";
    String pathToTest = "/invalid/" + variable;

    ServiceException exception =
        assertThrows(
            ServiceException.class, () -> getVariableFromPath(template, pathToTest, variableName));

    assertThat(exception.getErrorCode()).isEqualTo(INVALID_ARGUMENT);
    assertThat(exception.getErrorReason()).isEqualTo(INVALID_URL_PATH_OR_VARIABLE.name());
    assertThat(exception.getMessage()).isEqualTo("Invalid URL Path: '" + pathToTest + "'");
  }

  @Test
  public void getVariableFromPath_invalidPathEnd() {
    String variable = randomUUID().toString();
    String variableName = "test";
    String template = "/testResource/:test";
    String pathToTest = "/testResource/" + variable + "/";

    ServiceException exception =
        assertThrows(
            ServiceException.class, () -> getVariableFromPath(template, pathToTest, variableName));

    assertThat(exception.getErrorCode()).isEqualTo(INVALID_ARGUMENT);
    assertThat(exception.getErrorReason()).isEqualTo(INVALID_URL_PATH_OR_VARIABLE.name());
    assertThat(exception.getMessage()).isEqualTo("Invalid URL Path: '" + pathToTest + "'");
  }

  @Test
  public void validateHttpMethod_validMethodDoesNotThrowException() throws ServiceException {
    validateHttpMethod("POST", POST);
  }

  @Test
  public void validateHttpMethod_invalidMethod() {
    ServiceException exception =
        assertThrows(ServiceException.class, () -> validateHttpMethod("POST", GET));

    assertThat(exception.getErrorCode()).isEqualTo(INVALID_ARGUMENT);
    assertThat(exception.getErrorReason()).isEqualTo(INVALID_HTTP_METHOD.name());
    assertThat(exception.getMessage()).isEqualTo("Unsupported method: 'POST'");
  }
}
