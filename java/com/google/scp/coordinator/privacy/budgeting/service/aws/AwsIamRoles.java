/*
 * Copyright 2023 Google LLC
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

package com.google.scp.coordinator.privacy.budgeting.service.aws;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Utilities for working with AWS IAM roles */
public final class AwsIamRoles {

  private static final Pattern EXTRACT_IAM_ROLE_PATTERN =
      Pattern.compile("^arn:aws:sts::\\d+:assumed-role/(?<role>[\\w+=,.@-]+)/.*$");

  /**
   * Retrieves the IAM Role from the user ARN used for an "assume role" session. The user ARN can be
   * retrieved from the request context for API gateway requests.
   *
   * @param userArn The user ARN
   * @return the string of the IAM role
   * @throws IllegalArgumentException if the userArn does not follow the expected pattern.
   */
  public static String extractIamRoleFromAssumedRoleUserArn(String userArn) {
    Matcher iamRoleExtractor = EXTRACT_IAM_ROLE_PATTERN.matcher(userArn);

    if (!iamRoleExtractor.matches()) {
      throw new IllegalArgumentException("Unable to extract IAM role from userArn: " + userArn);
    }

    return iamRoleExtractor.group("role");
  }

  private AwsIamRoles() {}
}
