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

#include <fcntl.h>
#include <signal.h>

#include <string>

#include "logging.h"
#include "protocol.h"
#include "socket_vendor_server.h"

using google::scp::proxy::Endpoint;
using google::scp::proxy::LogError;
using google::scp::proxy::LogInfo;
using google::scp::proxy::SocketVendorServer;
using std::string;

int main(int argc, char* argv[]) {
  LogInfo("Nitro Enclave Proxy Socket Vendor (c) Google 2022.");
  string lockfile(kSocketVendorUdsPath);
  lockfile += ".lock";
  int lock_fd =
      open(lockfile.c_str(), O_CLOEXEC | O_CREAT | O_RDWR, S_IRWXU | S_IRGRP);
  if (lock_fd < 0) {
    LogError("Cannot open lock file: ", lockfile, ", error: ", strerror(errno));
    return 1;
  }
  struct flock lock;
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_len = 1;
  lock.l_start = 0;
  lock.l_pid = 0;
  int lock_result = fcntl(lock_fd, F_SETLK, &lock);
  if (lock_result < 0) {
    LogError("Cannot lock file ", lockfile,
             ", another socket vendor is probably running.\n"
             "(If so, it is probably OK.)");
    return 1;
  }

  {
    // Ignore SIGPIPE.
    struct sigaction act {};

    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, nullptr);
  }

  auto addr = GetProxyVsockAddr();
  Endpoint ep(&addr, sizeof(addr));

  SocketVendorServer server(kSocketVendorUdsPath, ep, 4);
  if (!server.Init()) {
    return 1;
  }
  server.Run();

  LogError("ERROR: A fatal error has occurred, terminating proxy instance");
  return 1;
}
