/*
 * Copyright 2024 Google LLC
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

package com.google.scp.shared.gcp.util;

import com.google.cloud.functions.HttpResponse;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.OutputStream;
import java.util.List;
import java.util.Map;
import java.util.Optional;

/**
 * A wrapper around the {@link HttpResponse} object. This is needed in order to fetch the return
 * status code as there is no getStatusCode method declared in the interface.
 */
public class HttpResponseWrapper implements HttpResponse {
  private Integer statusCode;

  private final HttpResponse httpResponse;

  private HttpResponseWrapper(HttpResponse httpResponse) {
    this.httpResponse = httpResponse;
  }

  @Override
  public void setStatusCode(int code) {
    this.httpResponse.setStatusCode(code);
    this.statusCode = code;
  }

  @Override
  public void setStatusCode(int code, String message) {
    this.httpResponse.setStatusCode(code, message);
    this.statusCode = code;
  }

  @Override
  public void setContentType(String contentType) {
    this.httpResponse.setContentType(contentType);
  }

  @Override
  public Optional<String> getContentType() {
    return this.httpResponse.getContentType();
  }

  @Override
  public void appendHeader(String header, String value) {
    this.httpResponse.appendHeader(header, value);
  }

  @Override
  public Map<String, List<String>> getHeaders() {
    return this.httpResponse.getHeaders();
  }

  @Override
  public OutputStream getOutputStream() throws IOException {
    return this.httpResponse.getOutputStream();
  }

  @Override
  public BufferedWriter getWriter() throws IOException {
    return this.httpResponse.getWriter();
  }

  public Integer getStatusCode() {
    return this.statusCode;
  }

  /**
   * @param httpResponse an instance of {@link com.google.cloud.functions.HttpResponse}
   * @return a HttpResponseWrapper
   */
  public static HttpResponseWrapper from(HttpResponse httpResponse) {
    if (httpResponse instanceof HttpResponseWrapper) {
      return (HttpResponseWrapper) httpResponse;
    }
    return new HttpResponseWrapper(httpResponse);
  }
}
