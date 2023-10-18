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

/* This code is based on the serdes implemenation of uvwasi for node JS
  https://github.com/nodejs/uvwasi/blob/main/src/wasi_serdes.c */

/*
MIT License

Copyright (c) 2019 Colin Ihrig and Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "deserializer.h"

using std::string;
using std::vector;

namespace google::scp::roma::wasm {

uint8_t WasmDeserializer::ReadUint8(void* mem_ptr, size_t mem_size,
                                    size_t offset) {
  if (offset >= mem_size) {
    // We got an invalid offset within the current memory
    return 0;
  }
  return static_cast<uint8_t*>(mem_ptr)[offset];
}

uint16_t WasmDeserializer::ReadUint16(void* mem_ptr, size_t mem_size,
                                      size_t offset) {
  if (offset + 1 >= mem_size) {
    // We got an invalid offset within the current memory
    return 0;
  }
  uint16_t lo = ReadUint8(mem_ptr, mem_size, offset);
  uint16_t hi = ReadUint8(mem_ptr, mem_size, offset + 1);
  return lo | (hi << 8);
}

uint32_t WasmDeserializer::ReadUint32(void* mem_ptr, size_t mem_size,
                                      size_t offset) {
  if (offset + 3 >= mem_size) {
    // We got an invalid offset within the current memory
    return 0;
  }
  uint32_t lo = ReadUint16(mem_ptr, mem_size, offset);
  uint32_t hi = ReadUint16(mem_ptr, mem_size, offset + 2);
  return lo | (hi << 16);
}

void WasmDeserializer::ReadRawString(void* mem_ptr, size_t mem_size,
                                     size_t offset, char* str, size_t str_len) {
  if (offset + str_len - 1 >= mem_size) {
    // We got an invalid offset within the current memory
    str[0] = '\0';
    return;
  }
  size_t local_offset = offset;
  for (size_t i = 0; i < str_len; i++) {
    str[i] = ReadUint8(mem_ptr, mem_size, local_offset++);
  }
}

void WasmDeserializer::ReadCustomString(void* mem_ptr, size_t mem_size,
                                        size_t offset, string& output) {
  // This means we can't even read the string pointer and length
  if (offset + 7 >= mem_size) {
    output.clear();
    return;
  }
  auto str_data_ptr = ReadUint32(mem_ptr, mem_size, offset);
  auto str_data_len = ReadUint32(mem_ptr, mem_size, offset + 4);
  // This means we can't read the string data
  if (str_data_ptr + str_data_len - 1 >= mem_size) {
    output.clear();
    return;
  }

  string temp_buffer;
  // Add space for the null terminator
  temp_buffer.reserve(str_data_len + 1);

  ReadRawString(mem_ptr, mem_size, str_data_ptr, &temp_buffer[0], str_data_len);

  if (temp_buffer[0] == '\0') {
    // The read string is empty
    output.clear();
    return;
  }

  output.assign(temp_buffer.data(), str_data_len);
}

void WasmDeserializer::ReadCustomListOfString(void* mem_ptr, size_t mem_size,
                                              size_t offset,
                                              vector<string>& output) {
  // This means we can't even read the list pointer and length
  if (offset + 7 >= mem_size) {
    output.clear();
    return;
  }

  auto ptr_to_list_of_str_ptrs = ReadUint32(mem_ptr, mem_size, offset);
  auto list_length = ReadUint32(mem_ptr, mem_size, offset + 4);

  // This means we can't read the list data
  if (ptr_to_list_of_str_ptrs + list_length - 1 >= mem_size) {
    output.clear();
    return;
  }

  offset = ptr_to_list_of_str_ptrs;

  for (size_t i = 0; i < list_length; i++) {
    if (offset + 3 >= mem_size) {
      // This means we can't even read the string pointer
      output.clear();
      return;
    }

    auto str_ptr = ReadUint32(mem_ptr, mem_size, offset);
    string str;
    ReadCustomString(mem_ptr, mem_size, str_ptr, str);
    output.push_back(str);
    // Add the size of the str_ptr to the offset
    offset += 4;
  }
}
}  // namespace google::scp::roma::wasm
