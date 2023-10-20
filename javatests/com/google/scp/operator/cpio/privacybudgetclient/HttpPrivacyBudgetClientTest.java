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

package com.google.scp.operator.cpio.privacybudgetclient;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.api.model.Code.PERMISSION_DENIED;
import static com.google.scp.shared.api.model.Code.UNKNOWN;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.common.collect.ImmutableList;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetRequest;
import com.google.scp.coordinator.privacy.budgeting.model.ConsumePrivacyBudgetResponse;
import com.google.scp.coordinator.privacy.budgeting.model.PrivacyBudgetUnit;
import com.google.scp.operator.cpio.privacybudgetclient.PrivacyBudgetClient.PrivacyBudgetClientException;
import com.google.scp.operator.cpio.privacybudgetclient.PrivacyBudgetClient.PrivacyBudgetServiceException;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.mapper.TimeObjectMapper;
import java.io.IOException;
import java.net.URI;
import java.time.Instant;
import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.ProtocolVersion;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpUriRequest;
import org.apache.http.entity.StringEntity;
import org.apache.http.message.BasicStatusLine;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public final class HttpPrivacyBudgetClientTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private HttpClient httpClient;

  private final ObjectMapper mapper = new TimeObjectMapper();

  Instant testInstant = Instant.now();

  @Test
  @SuppressWarnings("unchecked") // Ignore Http{Request,Response,Client} casting warnings.
  public void consumePrivacyBudget_exhaustedBudget() throws Exception {

    var response = buildResponse(getExhaustedBudget(), 200);
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);
    var expectedUri = URI.create("https://example.com/v1");
    var argument = ArgumentCaptor.forClass(HttpUriRequest.class);
    var service = new HttpPrivacyBudgetClient("https://example.com/v1", httpClient);

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        service.consumePrivacyBudget(getRequest());

    verify(httpClient).execute(argument.capture());
    assertThat(argument.getValue().getURI()).isEqualTo(expectedUri);
    assertThat(consumePrivacyBudgetResponse).isEqualTo(getExhaustedBudget());
  }

  @Test
  @SuppressWarnings("unchecked") // Ignore Http{Request,Response,Client} casting warnings.
  public void consumePrivacyBudget_nonExhaustedBudget() throws Exception {

    var response = buildResponse(getNotExhaustedBudget(), 200);
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);
    var expectedUri = URI.create("https://example.com/v1");
    var argument = ArgumentCaptor.forClass(HttpUriRequest.class);
    var service = new HttpPrivacyBudgetClient("https://example.com/v1", httpClient);

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        service.consumePrivacyBudget(getRequest());

    verify(httpClient).execute(argument.capture());
    assertThat(argument.getValue().getURI()).isEqualTo(expectedUri);
    assertThat(consumePrivacyBudgetResponse).isEqualTo(getNotExhaustedBudget());
  }

  @Test
  @SuppressWarnings("unchecked") // Ignore Http{Request,Response,Client} casting warnings.
  public void consumePrivacyBudget_status404() throws Exception {

    var response = buildResponse(getNotExhaustedBudget(), 404);
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);
    var expectedUri = URI.create("https://example.com/v1");
    var argument = ArgumentCaptor.forClass(HttpUriRequest.class);
    var service = new HttpPrivacyBudgetClient("https://example.com/v1", httpClient);

    assertThrows(
        PrivacyBudgetServiceException.class, () -> service.consumePrivacyBudget(getRequest()));

    verify(httpClient).execute(argument.capture());
    assertThat(argument.getValue().getURI()).isEqualTo(expectedUri);
  }

  @Test
  @SuppressWarnings("unchecked") // Ignore Http{Request,Response,Client} casting warnings.
  public void consumePrivacyBudget_emptyResponse() throws Exception {

    var response = buildResponse(null, 200);
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);
    var expectedUri = URI.create("https://example.com/v1");
    var argument = ArgumentCaptor.forClass(HttpUriRequest.class);
    var service = new HttpPrivacyBudgetClient("https://example.com/v1", httpClient);

    assertThrows(
        PrivacyBudgetServiceException.class, () -> service.consumePrivacyBudget(getRequest()));

    verify(httpClient).execute(argument.capture());
    assertThat(argument.getValue().getURI()).isEqualTo(expectedUri);
  }

  @Test
  public void consumePrivacyBudget_overSizeLimit() {
    var service = new HttpPrivacyBudgetClient("https://example.com/v1", httpClient);

    assertThrows(
        PrivacyBudgetClientException.class, () -> service.consumePrivacyBudget(getRequest(30001)));
  }

  @Test
  public void consumePrivacyBudget_maxSize() throws Exception {
    var response = buildResponse(getNotExhaustedBudget(), 200);
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);
    var expectedUri = URI.create("https://example.com/v1");
    var argument = ArgumentCaptor.forClass(HttpUriRequest.class);
    var service = new HttpPrivacyBudgetClient("https://example.com/v1", httpClient);

    ConsumePrivacyBudgetResponse consumePrivacyBudgetResponse =
        service.consumePrivacyBudget(getRequest());

    verify(httpClient).execute(argument.capture());
    assertThat(argument.getValue().getURI()).isEqualTo(expectedUri);
    assertThat(consumePrivacyBudgetResponse).isEqualTo(getNotExhaustedBudget());
  }

  @Test
  public void consumePrivacyBudget_forbidden() throws Exception {
    var service = new HttpPrivacyBudgetClient("https://example.com/v1", httpClient);
    var forbiddenResponse = "{\"message\":\"Forbidden\"}";
    var response = buildCustomResponse(forbiddenResponse, PERMISSION_DENIED.getHttpStatusCode());
    when(httpClient.execute(any(HttpUriRequest.class))).thenReturn(response);

    var exception =
        assertThrows(
            PrivacyBudgetServiceException.class, () -> service.consumePrivacyBudget(getRequest()));

    assertThat(exception).hasCauseThat().isInstanceOf(ServiceException.class);
    ServiceException e = (ServiceException) exception.getCause();
    assertThat(e.getMessage()).contains(forbiddenResponse);
    assertThat(e.getErrorCode()).isEqualTo(UNKNOWN); // Due to absence of code field in JSON
    assertThat(e.getErrorReason()).contains("");
  }

  private HttpResponse buildCustomResponse(String response, int statusCode) throws IOException {
    HttpResponse httpResponse = Mockito.mock(HttpResponse.class);
    HttpEntity httpEntity = new StringEntity(response == null ? "{}" : response);
    when(httpResponse.getEntity()).thenReturn(httpEntity);
    var protocol = new ProtocolVersion("HTTP", 1, 1);
    when(httpResponse.getStatusLine()).thenReturn(new BasicStatusLine(protocol, statusCode, ""));
    return httpResponse;
  }

  private HttpResponse buildResponse(ConsumePrivacyBudgetResponse response, int statusCode)
      throws IOException {
    String stringResponse = mapper.writeValueAsString(response == null ? "{}" : response);
    return buildCustomResponse(stringResponse, statusCode);
  }

  private ConsumePrivacyBudgetResponse getExhaustedBudget() {
    ImmutableList<PrivacyBudgetUnit> privacyBudgetUnits =
        ImmutableList.<PrivacyBudgetUnit>builder()
            .add(
                PrivacyBudgetUnit.builder()
                    .privacyBudgetKey("privacybudgetkey")
                    .reportingWindow(testInstant)
                    .build())
            .build();
    return ConsumePrivacyBudgetResponse.builder()
        .exhaustedPrivacyBudgetUnits(privacyBudgetUnits)
        .build();
  }

  private ConsumePrivacyBudgetResponse getNotExhaustedBudget() {
    return ConsumePrivacyBudgetResponse.builder()
        .exhaustedPrivacyBudgetUnits(ImmutableList.<PrivacyBudgetUnit>builder().build())
        .build();
  }

  private ConsumePrivacyBudgetRequest getRequest(int count) {
    var builder = ImmutableList.<PrivacyBudgetUnit>builder();
    for (int i = 0; i < count; ++i) {
      var key = String.format("privacybudgetkey-%d", i);
      builder.add(
          PrivacyBudgetUnit.builder().privacyBudgetKey(key).reportingWindow(testInstant).build());
    }
    var privacyBudgetUnits = builder.build();
    return ConsumePrivacyBudgetRequest.builder()
        .attributionReportTo("abc.com")
        .privacyBudgetUnits(privacyBudgetUnits)
        .build();
  }

  private ConsumePrivacyBudgetRequest getRequest() {
    return getRequest(1);
  }
}
