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

namespace google::scp::roma::wasm {
/**
 * @brief Helper methods to write data in the WASM memory space
 *
 */
class WasmSerializer {
 public:
  /**
   * @brief Write a uint8_t to memory
   *
   * @param mem_ptr
   * @param mem_size the size of the memory
   * @param offset
   * @param input
   * @return true on success or false
   */
  static bool WriteUint8(void* mem_ptr, size_t mem_size, size_t offset,
                         uint8_t input);

  /**
   * @brief Write a uint16_t to memory
   *
   * @param mem_ptr
   * @param mem_size the size of the memory
   * @param offset
   * @param input
   * @return true on success or false
   */
  static bool WriteUint16(void* mem_ptr, size_t mem_size, size_t offset,
                          uint16_t input);

  /**
   * @brief Write a uint32_t to memory
   *
   * @param mem_ptr
   * @param mem_size the size of the memory
   * @param offset
   * @param input
   * @return true on success or false
   */
  static bool WriteUint32(void* mem_ptr, size_t mem_size, size_t offset,
                          uint32_t input);

  /**
   * @brief Write a string to memory
   *
   * @param mem_ptr
   * @param mem_size the size of the memory
   * @param offset
   * @param str
   * @param str_len
   * @return true on success or false
   */
  static bool WriteRawString(void* mem_ptr, size_t mem_size, size_t offset,
                             const char* str, size_t str_len);

  /**
   * @brief Write a custom string to memory.
   * The memory representation will be that of
   * RomaWasmStringRepresentation (wasm_types.h) where we have a uint32_t
   * representing the pointer to the actual string data and another uint32_t
   * (size_t is 32 bits in WASM) representing the length of the string data.
   *
   * @param mem_ptr
   * @param mem_size the size of the memory
   * @param offset
   * @param str
   * @return The pointer to the written RomaWasmStringRepresentation
   * (offset) in the given memory segment
   */
  static uint32_t WriteCustomString(void* mem_ptr, size_t mem_size,
                                    size_t offset, const std::string& str);

  /**
   * @brief Write a custom list of strings to memory.
   * The memory representation will be that of
   * RomaWasmListOfStringRepresentation (wasm_types.h) where we have a uint32_t
   * representing the pointer to the strings and another uint32_t (size_t is 32
   * bits in WASM) representing the length of the list.
   *
   * @param mem_ptr
   * @param mem_size the size of the memory
   * @param offset
   * @param list The list of strings to be written to memory.
   * @return uint32_t
   */
  static uint32_t WriteCustomListOfString(void* mem_ptr, size_t mem_size,
                                          size_t offset,
                                          const std::vector<std::string>& list);
};
}  // namespace google::scp::roma::wasm
