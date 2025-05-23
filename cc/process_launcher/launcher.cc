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
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <iostream>

#include "cc/process_launcher/daemonizer/src/daemonizer.h"
#include "cc/public/core/interface/execution_result.h"

using privacy_sandbox::pbs_common::Daemonizer;

void TerminateSignalHandler(int signal_code) {
  fprintf(
      stderr,
      "Process Launcher: handled signal with code: %d. Ignoring the signal.\n",
      signal_code);
  // Do not terminate if termination signals are sent to this.
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    throw std::runtime_error("Must provide at least one argument.");
  }

  // Process launcher should not be terminated
  signal(SIGINT, TerminateSignalHandler);
  signal(SIGTERM, TerminateSignalHandler);
  signal(SIGHUP, TerminateSignalHandler);

  Daemonizer daemonizer(argc - 1, &argv[1]);
  auto result = daemonizer.Run();
  if (!result.Successful()) {
    std::cerr << "Process Launcher: Daemonizer::Run() exited with code: "
              << result.status_code;
  }

  std::cerr << "Process Launcher: Exiting" << std::endl;
  return 0;
}
