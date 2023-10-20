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

#include "string_util.h"

#include <list>
#include <string>

using std::list;
using std::string;

namespace google::scp::core::utils {
void SplitStringByDelimiter(const string& str, const string& delimiter,
                            list<string>& out) {
  auto start = 0U;
  auto end = str.find(delimiter);

  while (end != string::npos) {
    auto part = str.substr(start, end - start);
    out.push_back(part);
    start = end + delimiter.length();
    end = str.find(delimiter, start);
  }

  auto part = str.substr(start, end);
  out.push_back(part);
}
}  // namespace google::scp::core::utils
