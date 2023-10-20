package com.google.scp.shared.clients.configclient.gcp;

import static com.google.common.truth.Truth.assertThat;

import com.google.auth.oauth2.GoogleCredentials;
import java.io.IOException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class CredentialsHelperTest {

  @Test
  public void getAttestedCredentials_success() throws IOException {
    String wip = "wip";
    String sa = "serviceAccount";

    GoogleCredentials creds = CredentialsHelper.getAttestedCredentials(wip, sa);

    assertThat(creds).isNotNull();
  }
}
