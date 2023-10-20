// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// #include "core/logger/src/logger.h"

#include "core/logger/src/logger.h"

#include <gtest/gtest.h>

#include <memory>
#include <string_view>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/logger/mock/mock_logger.h"
#include "core/logger/src/log_utils.h"
#include "core/test/scp_test_base.h"

using google::scp::core::LogLevel;
using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;
using google::scp::core::logger::FromString;
using google::scp::core::logger::ToString;
using google::scp::core::logger::mock::MockLogger;
using std::string;
using std::vector;

namespace google::scp::core::test {
class LoggerTests : public ScpTestBase {
 protected:
  void SetUp() override {
    uuid = Uuid::GenerateUuid();
    uuid_str = ToString(uuid);

    parent_uuid = Uuid::GenerateUuid();
    parent_uuid_str = ToString(parent_uuid);

    correlation_id = Uuid::GenerateUuid();
    correlation_id_str = ToString(correlation_id);

    location =
        std::string(__FILE__) + ":" + __func__ + ":" + std::to_string(__LINE__);
  }

  Uuid uuid;
  string uuid_str;
  Uuid parent_uuid;
  string parent_uuid_str;
  Uuid correlation_id;
  string correlation_id_str;
  string location;
  const string component_name = "LoggerTest";
};

TEST_F(LoggerTests, LogDebug) {
  MockLogger logger;
  logger.Info(component_name, correlation_id, parent_uuid, uuid, location,
              "Message");

  auto logs = logger.GetMessages();
  EXPECT_EQ(logs.size(), 1);

  auto first_delim = logs[0].find("|");
  auto timestamp_string = logs[0].substr(0, first_delim);
  auto after_timestamp_string = logs[0].substr(first_delim);

  EXPECT_NO_THROW(std::stod(timestamp_string));
  EXPECT_EQ(after_timestamp_string, "|||LoggerTest|" + correlation_id_str +
                                        "|" + parent_uuid_str + "|" + uuid_str +
                                        "|" + location + "|32: Message");
}

TEST_F(LoggerTests, LogInfo) {
  MockLogger logger;
  logger.Debug(component_name, correlation_id, parent_uuid, uuid, location,
               "Message");

  auto logs = logger.GetMessages();
  EXPECT_EQ(logs.size(), 1);

  auto first_delim = logs[0].find("|");
  auto timestamp_string = logs[0].substr(0, first_delim);
  auto after_timestamp_string = logs[0].substr(first_delim);

  EXPECT_NO_THROW(std::stod(timestamp_string));
  EXPECT_EQ(after_timestamp_string, "|||LoggerTest|" + correlation_id_str +
                                        "|" + parent_uuid_str + "|" + uuid_str +
                                        "|" + location + "|16: Message");
}

TEST_F(LoggerTests, LogError) {
  MockLogger logger;
  logger.Error(component_name, correlation_id, parent_uuid, uuid, location,
               "Message");

  auto logs = logger.GetMessages();
  EXPECT_EQ(logs.size(), 1);

  auto first_delim = logs[0].find("|");
  auto timestamp_string = logs[0].substr(0, first_delim);
  auto after_timestamp_string = logs[0].substr(first_delim);

  EXPECT_NO_THROW(std::stod(timestamp_string));
  EXPECT_EQ(after_timestamp_string, "|||LoggerTest|" + correlation_id_str +
                                        "|" + parent_uuid_str + "|" + uuid_str +
                                        "|" + location + "|4: Message");
}

TEST_F(LoggerTests, LogWarning) {
  MockLogger logger;

  logger.Warning(component_name, correlation_id, parent_uuid, uuid, location,
                 "Message");

  auto logs = logger.GetMessages();
  EXPECT_EQ(logs.size(), 1);

  auto first_delim = logs[0].find("|");
  auto timestamp_string = logs[0].substr(0, first_delim);
  auto after_timestamp_string = logs[0].substr(first_delim);

  EXPECT_NO_THROW(std::stod(timestamp_string));
  EXPECT_EQ(after_timestamp_string, "|||LoggerTest|" + correlation_id_str +
                                        "|" + parent_uuid_str + "|" + uuid_str +
                                        "|" + location + "|8: Message");
}

TEST_F(LoggerTests, LogWithArgs) {
  MockLogger logger;

  logger.Warning(component_name, correlation_id, parent_uuid, uuid, location,
                 "Message %d %s", 1, "error");

  auto logs = logger.GetMessages();
  EXPECT_EQ(logs.size(), 1);

  auto first_delim = logs[0].find("|");
  auto timestamp_string = logs[0].substr(0, first_delim);
  auto after_timestamp_string = logs[0].substr(first_delim);

  EXPECT_EQ(after_timestamp_string, "|||LoggerTest|" + correlation_id_str +
                                        "|" + parent_uuid_str + "|" + uuid_str +
                                        "|" + location + "|8: Message 1 error");
}

TEST_F(LoggerTests, LogLevelToAndFromString) {
  vector<LogLevel> log_levels = {LogLevel::kAlert, LogLevel::kCritical,
                                 LogLevel::kDebug, LogLevel::kEmergency,
                                 LogLevel::kError, LogLevel::kInfo,
                                 LogLevel::kNone,  LogLevel::kWarning};

  for (auto log_level : log_levels) {
    EXPECT_EQ(FromString(ToString(log_level)), log_level);
  }
}
};  // namespace google::scp::core::test
