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

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "include/v8.h"
#include "roma/common/src/containers.h"

namespace google::scp::roma {
template <typename T>
struct TypeConverter {};

template <>
struct TypeConverter<std::string> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const std::string& val) {
    return v8::String::NewFromUtf8(isolate, val.data(),
                                   v8::NewStringType::kNormal,
                                   static_cast<uint32_t>(val.length()))
        .ToLocalChecked();
  }

  static bool FromV8(v8::Isolate* isolate, v8::Local<v8::Value> val,
                     std::string* out) {
    if (val.IsEmpty() || !val->IsString()) {
      return false;
    }

    v8::Local<v8::String> str = v8::Local<v8::String>::Cast(val);
    int length = str->Utf8Length(isolate);
    out->resize(length);
    str->WriteUtf8(isolate, &(*out)[0], length, NULL,
                   v8::String::NO_NULL_TERMINATION);

    return true;
  }
};

template <>
struct TypeConverter<std::vector<std::string>> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   std::vector<std::string>& val) {
    v8::Local<v8::Array> array = v8::Array::New(isolate, val.size());

    // Return an empty result if there was an error creating the array.
    if (array.IsEmpty()) {
      return v8::Local<v8::Array>();
    }

    for (size_t i = 0; i < val.size(); i++) {
      auto v8_str = TypeConverter<std::string>::ToV8(isolate, val.at(i));
      auto result = array->Set(isolate->GetCurrentContext(), i, v8_str);
      result.Check();
    }

    return array;
  }

  static bool FromV8(v8::Isolate* isolate, v8::Local<v8::Value> val,
                     std::vector<std::string>* out) {
    if (val.IsEmpty() || !val->IsArray()) {
      return false;
    }

    auto array = val.As<v8::Array>();
    for (size_t i = 0; i < array->Length(); i++) {
      auto item = array->Get(isolate->GetCurrentContext(), i).ToLocalChecked();
      std::string str;

      if (!TypeConverter<std::string>::FromV8(isolate, item, &str)) {
        out->clear();
        return false;
      }

      out->push_back(str);
    }

    return true;
  }
};

template <>
struct TypeConverter<common::Map<std::string, std::string>> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   common::Map<std::string, std::string>& val) {
    auto map = v8::Map::New(isolate);
    auto keys = val.Keys();

    for (auto& key : keys) {
      map->Set(isolate->GetCurrentContext(),
               TypeConverter<std::string>::ToV8(isolate, key),
               TypeConverter<std::string>::ToV8(isolate, val.Get(key)));
    }

    return map;
  }

  static bool FromV8(v8::Isolate* isolate, v8::Local<v8::Value> val,
                     common::Map<std::string, std::string>* out) {
    if (val.IsEmpty() || !val->IsMap()) {
      return false;
    }

    auto v8_map = val.As<v8::Map>();
    // This turns the map into an array of size Size()*2, where index N is a
    // key, and N+1 is the value for the given key.
    auto map_as_array = v8_map->AsArray();

    for (auto i = 0; i < map_as_array->Length(); i += 2) {
      auto key_index = i;
      auto value_index = i + 1;

      auto key = map_as_array->Get(isolate->GetCurrentContext(), key_index)
                     .ToLocalChecked();
      auto value = map_as_array->Get(isolate->GetCurrentContext(), value_index)
                       .ToLocalChecked();

      std::string key_str;
      std::string val_str;
      bool key_conversion =
          TypeConverter<std::string>::FromV8(isolate, key, &key_str);
      bool value_conversion =
          TypeConverter<std::string>::FromV8(isolate, value, &val_str);

      if (!key_conversion || !value_conversion) {
        out->Clear();
        return false;
      }

      out->Set(key_str, val_str);
    }

    return true;
  }
};

template <>
struct TypeConverter<absl::flat_hash_map<std::string, std::string>> {
  static v8::Local<v8::Value> ToV8(
      v8::Isolate* isolate,
      absl::flat_hash_map<std::string, std::string>& val) {
    auto map = v8::Map::New(isolate);

    for (const auto& kvp : val) {
      map->Set(isolate->GetCurrentContext(),
               TypeConverter<std::string>::ToV8(isolate, kvp.first),
               TypeConverter<std::string>::ToV8(isolate, kvp.second));
    }

    return map;
  }

  static bool FromV8(v8::Isolate* isolate, v8::Local<v8::Value> val,
                     absl::flat_hash_map<std::string, std::string>* out) {
    if (!out || val.IsEmpty() || !val->IsMap()) {
      return false;
    }

    auto v8_map = val.As<v8::Map>();
    // This turns the map into an array of size Size()*2, where index N is a
    // key, and N+1 is the value for the given key.
    auto map_as_array = v8_map->AsArray();

    for (auto i = 0; i < map_as_array->Length(); i += 2) {
      auto key_index = i;
      auto value_index = i + 1;

      auto key = map_as_array->Get(isolate->GetCurrentContext(), key_index)
                     .ToLocalChecked();
      auto value = map_as_array->Get(isolate->GetCurrentContext(), value_index)
                       .ToLocalChecked();

      std::string key_str;
      std::string val_str;
      bool key_conversion =
          TypeConverter<std::string>::FromV8(isolate, key, &key_str);
      bool value_conversion =
          TypeConverter<std::string>::FromV8(isolate, value, &val_str);

      if (!key_conversion || !value_conversion) {
        out->clear();
        return false;
      }

      (*out)[key_str] = val_str;
    }

    return true;
  }
};

template <>
struct TypeConverter<uint32_t> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate, const uint32_t& val) {
    return v8::Integer::NewFromUnsigned(isolate, val);
  }

  static bool FromV8(v8::Isolate* isolate, v8::Local<v8::Value> val,
                     uint32_t* out) {
    if (val.IsEmpty() || !val->IsUint32()) {
      return false;
    }

    *out = val.As<v8::Uint32>()->Value();

    return true;
  }
};

template <>
struct TypeConverter<uint8_t*> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate, const uint8_t* data,
                                   size_t data_size) {
    v8::Local<v8::ArrayBuffer> buffer =
        v8::ArrayBuffer::New(isolate, data_size);
    memcpy(buffer->Data(), data, data_size);
    return v8::Uint8Array::New(buffer, 0, data_size);
  }

  static bool FromV8(v8::Isolate* isolate, v8::Local<v8::Value> val,
                     uint8_t* out, size_t out_buffer_size) {
    if (val.IsEmpty() || !val->IsUint8Array() || out == nullptr) {
      return false;
    }

    auto val_array = val.As<v8::Uint8Array>();
    // The buffer size needs to match the size of the data we're trying to
    // write.
    if (val_array->Length() != out_buffer_size) {
      return false;
    }

    memcpy(out, val_array->Buffer()->Data(), val_array->Length());
    return true;
  }
};
}  // namespace google::scp::roma
