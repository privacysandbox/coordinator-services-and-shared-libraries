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

package com.google.scp.shared.testutils.common;

import com.google.devtools.build.runfiles.Runfiles;
import com.google.inject.Inject;
import java.io.IOException;
import java.nio.file.Path;

/**
 * Utility class for resolving files needed at runtime. Used for resolving cross-repo dependencies
 * during testing.
 *
 * <p>Background: yaqs/5881413041299390464
 */
public class RepoUtil {

  // Name of the top level runfiles directory to resolve the file path from. Should be "__main__" if
  // run from this repo or the imported bazel repo name if started from another.
  private String repoName;

  @Inject
  public RepoUtil() {
    this.repoName = "__main__";
  }

  public RepoUtil(String repoName) {
    this.repoName = repoName;
  }

  public Path getFilePath(String relativePath) {
    Runfiles runfiles;
    try {
      runfiles = Runfiles.create();
    } catch (IOException e) {
      throw new IllegalStateException("Failed to create runfiles", e);
    }

    var path = Path.of(runfiles.rlocation(String.format("%s/%s", repoName, relativePath)));
    return path;
  }
}
