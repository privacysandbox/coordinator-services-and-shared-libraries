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

#pragma once

#include <stddef.h>

#include <string>
#include <vector>

namespace google::scp::roma::wasm {
struct RomaWasmStringRepresentation {
  char* str;
  size_t len;

  RomaWasmStringRepresentation() : str(nullptr), len(0) {}

  ~RomaWasmStringRepresentation() {
    if (str != nullptr) {
      delete[] str;
    }
  }

  /**
   * @brief Get the size that a string will take in the WASM memory. This will
   * be the size of the actual string data, and the size of the struct members.
   *
   * @param s The string
   * @return size_t
   */
  static size_t ComputeMemorySizeFor(const std::string& s) {
    // 4 for the char pointer, and 4 for the size_t (size_t is 32 bits in WASM)
    return s.length() + 4 + 4;
  }
};

struct RomaWasmListOfStringRepresentation {
  // Array of pointers
  RomaWasmStringRepresentation** list;
  size_t len;

  RomaWasmListOfStringRepresentation() : list(nullptr), len(0) {}

  ~RomaWasmListOfStringRepresentation() {
    if (list != nullptr) {
      for (size_t i = 0; i < len; i++) {
        delete list[i];
      }
      delete[] list;
    }
  }

  /**
   * @brief Get the size that a list of string will take in the WASM memory.
   * This will be the size of the actual string data, and the size of the struct
   * members.
   *
   * @param s The string
   * @return size_t
   */
  static size_t ComputeMemorySizeFor(const std::vector<std::string>& list) {
    size_t size = 0;
    // Space used by each string
    for (size_t i = 0; i < list.size(); i++) {
      size += RomaWasmStringRepresentation::ComputeMemorySizeFor(list.at(i));
    }

    // Space used by the pointers to the strings
    size += list.size() * 4;
    // 4 for the list pointer, and 4 for the size_t (size_t is 32 bits in WASM)
    size += 4 + 4;

    return size;
  }
};
}  // namespace google::scp::roma::wasm
