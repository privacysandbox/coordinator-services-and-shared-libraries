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

#include <cstring>
#include <string>
#include <vector>

// String representation to define memory layout
struct StringRepresentationStruct {
  char* str;
  size_t len;

  explicit StringRepresentationStruct(std::string& s) {
    str = strdup(s.c_str());
    len = s.length();
  }
};

// List of string representation to define memory layout
struct ListOfStringRepresentationStruct {
  StringRepresentationStruct** list;
  size_t len;

  // Helper constructor
  explicit ListOfStringRepresentationStruct(std::vector<std::string>& vec) {
    auto new_vec = new std::vector<StringRepresentationStruct*>();
    for (size_t i = 0; i < vec.size(); i++) {
      new_vec->push_back(new StringRepresentationStruct(vec.at(i)));
    }

    list = new_vec->data();
    len = new_vec->size();
  }
};

// Extern C to avoid name mangling of the Handler, so we can get a predictable
// name
extern "C" ListOfStringRepresentationStruct* Handler(
    ListOfStringRepresentationStruct* input) {
  if (!input || input->len == 0) {
    return NULL;
  }

  std::vector<std::string> output_vector;
  for (size_t i = 0; i < input->len; i++) {
    auto item = input->list[i];
    output_vector.emplace_back(item->str, item->len);
  }

  // Add to the values that will be returned
  output_vector.push_back("String from Cpp1");
  output_vector.push_back("String from Cpp2");

  ListOfStringRepresentationStruct* output =
      new ListOfStringRepresentationStruct(output_vector);

  return output;
}
