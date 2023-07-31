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

#include "serializer.h"

#include "wasm_types.h"

using std::string;
using std::vector;

namespace google::scp::roma::wasm {
bool WasmSerializer::WriteUint8(void* mem_ptr, size_t mem_size, size_t offset,
                                uint8_t input) {
  if (offset >= mem_size) {
    return false;
  }
  static_cast<uint8_t*>(mem_ptr)[offset] = input;
  return true;
}

bool WasmSerializer::WriteUint16(void* mem_ptr, size_t mem_size, size_t offset,
                                 uint16_t input) {
  if (offset + 1 >= mem_size) {
    return false;
  }

  return WriteUint8(mem_ptr, mem_size, offset, (uint8_t)input) &&
         WriteUint8(mem_ptr, mem_size, offset + 1, input >> 8);
}

bool WasmSerializer::WriteUint32(void* mem_ptr, size_t mem_size, size_t offset,
                                 uint32_t input) {
  if (offset + 3 >= mem_size) {
    return false;
  }

  return WriteUint16(mem_ptr, mem_size, offset, (uint16_t)input) &&
         WriteUint16(mem_ptr, mem_size, offset + 2, input >> 16);
}

bool WasmSerializer::WriteRawString(void* mem_ptr, size_t mem_size,
                                    size_t offset, const char* str,
                                    size_t str_len) {
  if (offset + str_len - 1 >= mem_size) {
    return false;
  }
  size_t local_offset = offset;
  for (size_t i = 0; i < str_len; i++) {
    auto worked = WriteUint8(mem_ptr, mem_size, local_offset++,
                             static_cast<uint8_t>(str[i]));
    if (!worked) {
      return false;
    }
  }

  return true;
}

uint32_t WasmSerializer::WriteCustomString(void* mem_ptr, size_t mem_size,
                                           size_t offset, const string& str) {
  auto str_data_ptr = offset;
  auto worked = WriteRawString(mem_ptr, mem_size, str_data_ptr, str.c_str(),
                               str.length());
  if (!worked) {
    return UINT32_MAX;
  }

  auto struct_offset = str_data_ptr + str.length();
  worked = WriteUint32(mem_ptr, mem_size, struct_offset, str_data_ptr);
  worked =
      worked && WriteUint32(mem_ptr, mem_size, struct_offset + 4, str.length());

  if (!worked) {
    return UINT32_MAX;
  }

  return struct_offset;
}

uint32_t WasmSerializer::WriteCustomListOfString(void* mem_ptr, size_t mem_size,
                                                 size_t offset,
                                                 const vector<string>& list) {
  vector<uint32_t> string_ptrs;

  for (size_t i = 0; i < list.size(); i++) {
    auto str = list.at(i);

    auto str_ptr = WriteCustomString(mem_ptr, mem_size, offset, str);
    if (str_ptr == UINT32_MAX) {
      return UINT32_MAX;
    }
    string_ptrs.push_back(str_ptr);
    offset += RomaWasmStringRepresentation::ComputeMemorySizeFor(str);
  }

  auto ptr_to_array_of_str_ptrs = offset;

  for (size_t i = 0; i < string_ptrs.size(); i++) {
    auto worked = WriteUint32(mem_ptr, mem_size, offset, string_ptrs.at(i));
    if (!worked) {
      return UINT32_MAX;
    }
    offset += 4;
  }

  auto struct_offset = offset;

  // Write the pointer to the array of string pointers
  auto worked =
      WriteUint32(mem_ptr, mem_size, offset, ptr_to_array_of_str_ptrs);
  offset += 4;
  // Write the size of the list
  worked = worked && WriteUint32(mem_ptr, mem_size, offset, string_ptrs.size());

  if (!worked) {
    return UINT32_MAX;
  }

  return struct_offset;
}
}  // namespace google::scp::roma::wasm
