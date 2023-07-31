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
#include <stdint.h>

#include <string>
#include <vector>

#include "wasm_types.h"

namespace google::scp::roma::wasm {
/**
 * @brief Helper methods to read data from the WASM memory space
 *
 */
class WasmDeserializer {
 public:
  /**
   * @brief Read a uint8_t from memory
   *
   * @param mem_ptr
   * @param mem_size the size of the memory
   * @param offset
   * @return uint8_t
   */
  static uint8_t ReadUint8(void* mem_ptr, size_t mem_size, size_t offset);

  /**
   * @brief Read a uint16_t from memory
   *
   * @param mem_ptr
   * @param mem_size the size of the memory
   * @param offset
   * @return uint16_t
   */
  static uint16_t ReadUint16(void* mem_ptr, size_t mem_size, size_t offset);

  /**
   * @brief Read a uint32_t from memory
   *
   * @param mem_ptr
   * @param mem_size the size of the memory
   * @param offset
   * @return uint32_t
   */
  static uint32_t ReadUint32(void* mem_ptr, size_t mem_size, size_t offset);

  /**
   * @brief Read a string from memory. Does not null terminate the string.
   * It will just read the requested length, if within bounds or set the output
   * str to '\0'
   *
   * @param mem_ptr
   * @param mem_size the size of the memory
   * @param offset
   * @param[out] str The string to return
   * @param str_len The length of the string to read
   */
  static void ReadRawString(void* mem_ptr, size_t mem_size, size_t offset,
                            char* str, size_t str_len);

  /**
   * @brief Read a string representation from the memory segment into a string
   *
   * @param mem_ptr The memory segment to read from
   * @param mem_size the size of the memory
   * @param offset The pointer within the memory segment where the data can be
   * found
   * @param[out] output The read string.
   */
  static void ReadCustomString(void* mem_ptr, size_t mem_size, size_t offset,
                               std::string& output);

  /**
   * @brief Read a list of string representation from the memory segment into a
   * string
   *
   * @param mem_ptr
   * @param mem_size the size of the memory
   * @param offset
   * @param[out] output The read list of string
   */
  static void ReadCustomListOfString(void* mem_ptr, size_t mem_size,
                                     size_t offset,
                                     std::vector<std::string>& output);
};
}  // namespace google::scp::roma::wasm
