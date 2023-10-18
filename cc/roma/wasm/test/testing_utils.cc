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
#include "testing_utils.h"

#include <fstream>

#include "absl/log/check.h"

using std::ifstream;
using std::runtime_error;
using std::vector;

namespace google::scp::roma::wasm::testing {
std::vector<char> WasmTestingUtils::LoadWasmFile(const std::string& file_path) {
  vector<char> file_contents;

  ifstream input_file(file_path);
  CHECK(input_file.good()) << "File: " + file_path + " does not exist.";

  input_file.seekg(0, std::ios::end);
  size_t filesize = input_file.tellg();
  input_file.seekg(0, std::ios::beg);
  file_contents.resize(filesize / sizeof(uint8_t));
  input_file.read(file_contents.data(), filesize);

  return file_contents;
}
}  // namespace google::scp::roma::wasm::testing
