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

#include "proxy/src/config.h"

#include <getopt.h>

#include <string>

#include "proxy/src/logging.h"

namespace google::scp::proxy {
Config Config::Parse(int argc, char* argv[]) {
  Config config;

  // clang-format off
  static struct option long_options[] {
    { "tcp", no_argument, 0, 't'},
    { "port", required_argument, 0, 'p'},
    { "buffer_size", required_argument, 0, 'b'},
    {0, 0, 0, 0}
  };

  // clang-format on

  while (true) {
    int opt_idx = 0;
    int c = getopt_long(argc, argv, "tp:b:", long_options, &opt_idx);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 0: {
        break;
      }
      case 't': {
        config.vsock_ = false;
        break;
      }
      case 'p': {
        char* endptr;
        std::string port_str(optarg);
        auto port = strtoul(port_str.c_str(), &endptr, 10);
        if (port > UINT16_MAX) {
          LogError("ERROR: Invalid port number: ", port_str);
          exit(1);
        }
        config.socks5_port_ = static_cast<uint16_t>(port);
        break;
      }
      case 'b': {
        char* endptr;
        std::string bs_str(optarg);
        auto bs = strtoul(bs_str.c_str(), &endptr, 10);
        if (bs == 0) {
          LogError("ERROR: Invalid buffer size: ", bs_str);
          exit(1);
        }
        config.buffer_size_ = bs;
        break;
      }
      case '?': {  // Unrecognized option. Error should be printed already.
        config.bad_ = true;
        break;
      }
      default: {
        LogError("ERROR: unknown error.");
        abort();
      }
    }
  }
  return config;
}
}  // namespace google::scp::proxy
