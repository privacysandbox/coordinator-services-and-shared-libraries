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

#include <iostream>

#include "config.h"

namespace google::scp::proxy {

template <typename Stream, typename T>
void LogToStream(Stream& stream, const T& t) {
  stream << t << std::endl;
}

template <typename Stream, typename Arg0, typename... Args>
void LogToStream(Stream& stream, const Arg0& arg0, const Args&... args) {
  stream << arg0;
  LogToStream(stream, args...);
}

template <typename... Args>
void LogError(const Args&... args) {
  LogToStream(std::cerr, args...);
}

template <typename... Args>
void LogInfo(const Args&... args) {
  LogToStream(std::cout, args...);
}
}  // namespace google::scp::proxy
