// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CONFIG_H_
#define CONFIG_H_

#include <stddef.h>
#include <stdint.h>

namespace google::scp::proxy {

// The configurations of the proxy
struct Config {
  static constexpr uint16_t kDefaultPort = 8888;
  static constexpr size_t kDefaultBufferSize = 65536;

  Config()
      : buffer_size_(kDefaultBufferSize),
        socks5_port_(kDefaultPort),
        vsock_(true),
        bad_(false) {}

  // Parse the command line arguments and get a Config object.
  static Config Parse(int argc, char* argv[]);

  // The buffer size to use for internal logic.
  size_t buffer_size_;
  // Port that socks5 server listens on.
  uint16_t socks5_port_;
  // True if listen on vsock. Otherwise on TCP.
  bool vsock_;
  // If the config is bad.
  bool bad_;
};
}  // namespace google::scp::proxy

#endif  // CONFIG_H_
