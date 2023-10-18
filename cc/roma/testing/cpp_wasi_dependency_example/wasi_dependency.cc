/*
 * Copyright 2023 Google LLC
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

#include <string>

// Extern C to avoid name mangling of the Handler, so we can get a predictable
// name
extern "C" int Handler(int input) {
  std::string some_string = "hello";
  some_string += std::to_string(input);

  if (some_string == "hello1") {
    return 0;
  }

  return 1;
}
