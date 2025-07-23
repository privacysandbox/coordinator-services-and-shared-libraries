/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.shared.gcp.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.auth.oauth2.IdToken;
import com.google.auth.oauth2.IdTokenProvider;
import java.io.IOException;
import java.util.Optional;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class GcpHttpInterceptorUtilTest {

  @Test
  public void getServiceAccountEmail_success() throws IOException {
    // A base64 encoded JWT with the payload: {"email": "test-email@example.com"}
    String fakeToken =
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJlbWFpbCI6InRlc3QtZW1haWxAZXhhbXBsZS5jb20iLCJpYXQiOjE1MTYyMzkwMjJ9.5_Y7HnI4qgo3A-6a9J2ot_i_sK1LQLA4g_oYc3-J9L4";
    GcpHttpInterceptorUtil.GcpHttpInterceptor interceptor = createInterceptor(fakeToken);

    Optional<String> email = interceptor.getServiceAccountEmail();

    assertTrue(email.isPresent());
    assertEquals("test-email@example.com", email.get());
  }

  @Test
  public void getServiceAccountEmail_noEmail_returnsEmpty() throws IOException {
    // A base64 encoded JWT with the payload: {}
    String fakeToken =
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.e30.t-IDcSemACt8xhtWdJg0PVdC903FKvmLXloUa2pyHI";
    GcpHttpInterceptorUtil.GcpHttpInterceptor interceptor = createInterceptor(fakeToken);

    Optional<String> email = interceptor.getServiceAccountEmail();

    assertFalse(email.isPresent());
  }

  private GcpHttpInterceptorUtil.GcpHttpInterceptor createInterceptor(String token)
      throws IOException {
    GoogleCredentials mockCredentials =
        mock(GoogleCredentials.class, withSettings().extraInterfaces(IdTokenProvider.class));
    IdToken mockIdToken = mock(IdToken.class);

    when(((IdTokenProvider) mockCredentials).idTokenWithAudience(any(), any()))
        .thenReturn(mockIdToken);
    when(mockIdToken.getTokenValue()).thenReturn(token);

    return new GcpHttpInterceptorUtil.GcpHttpInterceptor(mockCredentials, "test-audience");
  }
}
