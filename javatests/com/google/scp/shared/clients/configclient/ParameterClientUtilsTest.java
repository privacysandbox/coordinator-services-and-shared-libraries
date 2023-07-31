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

package com.google.scp.shared.clients.configclient;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import com.google.scp.shared.clients.configclient.ParameterClient.ParameterClientException;
import java.util.Collections;
import java.util.Optional;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class ParameterClientUtilsTest {

  @Test
  public void getStorageParameterName_withPrefixAndEnvironment() throws Exception {
    String value =
        ParameterClientUtils.getStorageParameterName(
            "TEST", Optional.of("scp"), Optional.of("environment"));
    assertThat(value).isEqualTo("scp-environment-TEST");
  }

  @Test
  public void getStorageParameterName_withPrefixOnly() throws Exception {
    String value =
        ParameterClientUtils.getStorageParameterName("param", Optional.of("scp"), Optional.empty());
    assertThat(value).isEqualTo("scp-param");
  }

  @Test
  public void getStorageParameterName_withEnvironmentOnly() throws Exception {
    String value =
        ParameterClientUtils.getStorageParameterName(
            "param", Optional.empty(), Optional.of("environment"));
    assertThat(value).isEqualTo("environment-param");
  }

  @Test
  public void getStorageParameterName_withParameterOnly() throws Exception {
    String value =
        ParameterClientUtils.getStorageParameterName("param", Optional.empty(), Optional.empty());
    assertThat(value).isEqualTo("param");
  }

  @Test
  public void getStorageParameterName_withoutParameterThrows() throws Exception {
    assertThrows(
        ParameterClientException.class,
        () -> ParameterClientUtils.getStorageParameterName("", Optional.empty(), Optional.empty()));
  }

  @Test
  public void getStorageParameterName_withInvalidParameterThrows() throws Exception {
    assertThrows(
        ParameterClientException.class,
        () ->
            ParameterClientUtils.getStorageParameterName(
                "invalid@", Optional.of("prefix"), Optional.of("environment")));
  }

  @Test
  public void getStorageParameterName_withInvalidPrefixThrows() throws Exception {
    assertThrows(
        ParameterClientException.class,
        () ->
            ParameterClientUtils.getStorageParameterName(
                "param", Optional.of("/invalid/"), Optional.empty()));
  }

  @Test
  public void getStorageParameterName_withInvalidEnvironmentThrows() throws Exception {
    assertThrows(
        ParameterClientException.class,
        () ->
            ParameterClientUtils.getStorageParameterName(
                "param", Optional.of("prefix"), Optional.of("'invalid'")));
  }

  @Test
  public void validateParameterName_withValidParameters() throws Exception {
    ParameterClientUtils.validateParameterName("scp-environment-TEST_PARAM_1");
  }

  @Test
  public void validateParameterName_atLengthLimit() throws Exception {
    ParameterClientUtils.validateParameterName(String.join("", Collections.nCopies(255, "a")));
  }

  @Test
  public void validateParameterName_withEmptyStringThrows() throws Exception {
    assertThrows(
        ParameterClientException.class, () -> ParameterClientUtils.validateParameterName(""));
  }

  @Test
  public void validateParameterName_pastLengthLimitThrows() throws Exception {
    assertThrows(
        ParameterClientException.class,
        () ->
            ParameterClientUtils.validateParameterName(
                String.join("", Collections.nCopies(256, "a"))));
  }

  @Test
  public void validateParameterName_withSpecialCharactersThrows() throws Exception {
    assertThrows(
        ParameterClientException.class,
        () -> ParameterClientUtils.validateParameterName("/scp/environment/test"));
  }
}
