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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/util/json_util.h>

#include "absl/status/status_matchers.h"
#include "cc/core/common/uuid/src/error_codes.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/type_def.h"
#include "cc/core/test/utils/proto_test_utils.h"
#include "cc/pbs/front_end_service/src/error_codes.h"
#include "cc/pbs/front_end_service/src/front_end_utils.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "proto/pbs/api/v1/api.pb.h"

namespace google::scp::pbs {
namespace {

using ::absl_testing::IsOk;
using ::google::protobuf::util::JsonStringToMessage;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::SuccessExecutionResult;
using ::privacy_sandbox::pbs_common::Uuid;
using ::google::scp::core::test::EqualsProto;
using ::google::scp::core::test::ResultIs;
using ::privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest;
using ::privacy_sandbox::pbs_common::Byte;
using ::privacy_sandbox::pbs_common::BytesBuffer;
using ::privacy_sandbox::pbs_common::HttpHeaders;
using ::privacy_sandbox::pbs_common::kClaimedIdentityHeader;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::Return;

constexpr absl::string_view kAuthorizedDomain = "https://fake.com";
constexpr absl::string_view kTransactionOriginWithSubdomain =
    "https://subdomain.fake.com";
constexpr absl::string_view kTransactionOriginWithoutSubdomain =
    "https://fake.com";
const absl::string_view kBudgetTypeBinaryBudget =
    ConsumePrivacyBudgetRequest::PrivacyBudgetKey::BudgetType_Name(
        ConsumePrivacyBudgetRequest::PrivacyBudgetKey::
            BUDGET_TYPE_BINARY_BUDGET);

// Mock class with mock method to test ParseCommonV2TransactionRequestBody
class MockKeyBodyProcessor {
 public:
  MOCK_METHOD(ExecutionResult, ProcessKeyBody,
              (const nlohmann::json&, size_t, absl::string_view,
               absl::string_view));

  MOCK_METHOD(ExecutionResult, ProcessKeyBody,
              (const privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest::
                   PrivacyBudgetKey&,
               size_t, absl::string_view));
};

TEST(ParseBeginTransactionTest, ParseBeginTransactionV2RequestSuccess) {
  std::string begin_transaction_body = R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        },
        {
          "key": "124",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        },
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T08:20:50.52Z"
        }
      ]
    }
  ]
})";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      std::string(kAuthorizedDomain),
      std::string(kTransactionOriginWithoutSubdomain), bytes_buffer,
      consume_budget_metadata_list);
  EXPECT_SUCCESS(execution_result);
  EXPECT_EQ(consume_budget_metadata_list.size(), 4);

  auto it = consume_budget_metadata_list.begin();
  EXPECT_EQ(*it->budget_key_name, "http://a.fake.com/123");
  EXPECT_EQ(it->token_count, 1);
  EXPECT_EQ(it->time_bucket, 1576048850000000000);
  ++it;
  EXPECT_EQ(*it->budget_key_name, "http://a.fake.com/124");
  EXPECT_EQ(it->token_count, 1);
  EXPECT_EQ(it->time_bucket, 1576048850000000000);
  ++it;
  EXPECT_EQ(*it->budget_key_name, "http://b.fake.com/456");
  EXPECT_EQ(it->token_count, 1);
  EXPECT_EQ(it->time_bucket, 1576135250000000000);
}

TEST(ParseBeginTransactionTest, V2RequestWithUnauthorizedReportingOrigin) {
  std::string begin_transaction_body = R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "reporting_origin": "http://b.shoe.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }
      ]
    }
  ]
})";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      std::string(kAuthorizedDomain),
      std::string(kTransactionOriginWithoutSubdomain), bytes_buffer,
      consume_budget_metadata_list);
  EXPECT_THAT(
      execution_result,
      ResultIs(FailureExecutionResult(
          google::scp::core::errors::
              SC_PBS_FRONT_END_SERVICE_REPORTING_ORIGIN_NOT_BELONG_TO_SITE)));
  EXPECT_EQ(consume_budget_metadata_list.size(), 0);
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionV2RequestWithoutData) {
  std::string begin_transaction_body = R"({
     "v": "2.0",
   })";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      std::string(kAuthorizedDomain),
      std::string(kTransactionOriginWithoutSubdomain), bytes_buffer,
      consume_budget_metadata_list);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionV2RequestInvalidJson) {
  std::string begin_transaction_body = R"({
     "invalid"
   })";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      std::string(kAuthorizedDomain),
      std::string(kTransactionOriginWithoutSubdomain), bytes_buffer,
      consume_budget_metadata_list);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseBeginTransactionTest,
     ParseBeginTransactionV2RequestWithoutReportingOrigin) {
  std::string begin_transaction_body = R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }
      ]
    }
  ]
})";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      std::string(kAuthorizedDomain),
      std::string(kTransactionOriginWithoutSubdomain), bytes_buffer,
      consume_budget_metadata_list);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionV2RequestWithoutKeys) {
  std::string begin_transaction_body = R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
    }
  ]
})";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      std::string(kAuthorizedDomain),
      std::string(kTransactionOriginWithoutSubdomain), bytes_buffer,
      consume_budget_metadata_list);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseBeginTransactionTest,
     ParseBeginTransactionV2RequestWithTwoEqualsReportingOrigin) {
  std::string begin_transaction_body = R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }
      ]
    }
  ]
})";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      std::string(kAuthorizedDomain),
      std::string(kTransactionOriginWithoutSubdomain), bytes_buffer,
      consume_budget_metadata_list);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionV2RequestWithoutKey) {
  std::string begin_transaction_body = R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }
      ]
    }
  ]
})";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      std::string(kAuthorizedDomain),
      std::string(kTransactionOriginWithoutSubdomain), bytes_buffer,
      consume_budget_metadata_list);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionV2RequestWithoutToken) {
  std::string begin_transaction_body = R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "key": "456",
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }
      ]
    }
  ]
})";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      std::string(kAuthorizedDomain),
      std::string(kTransactionOriginWithoutSubdomain), bytes_buffer,
      consume_budget_metadata_list);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseBeginTransactionTest,
     ParseBeginTransactionV2RequestWithoutReportingTime) {
  std::string begin_transaction_body = R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
        }
      ]
    }
  ]
})";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      std::string(kAuthorizedDomain),
      std::string(kTransactionOriginWithoutSubdomain), bytes_buffer,
      consume_budget_metadata_list);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseBeginTransactionTest,
     ParseBeginTransactionV2RequestWithInvalidReportingTime) {
  std::string begin_transaction_body = R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "invalid"
        }
      ]
    }
  ]
})";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      std::string(kAuthorizedDomain),
      std::string(kTransactionOriginWithoutSubdomain), bytes_buffer,
      consume_budget_metadata_list);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
}

TEST(ParseBeginTransactionTest,
     ParseBeginTransactionV2RequestWithEqualsBudgetKey) {
  std::string begin_transaction_body = R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        },
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:51.53Z"
        }
      ]
    }
  ]
})";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      std::string(kAuthorizedDomain),
      std::string(kTransactionOriginWithoutSubdomain), bytes_buffer,
      consume_budget_metadata_list);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer) {
  BytesBuffer bytes_buffer;
  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer1) {
  BytesBuffer bytes_buffer(120);
  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer2) {
  std::string begin_transaction_body("{}");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer3) {
  std::string begin_transaction_body("{ \"v\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer4) {
  std::string begin_transaction_body("{ \"v\": \"\", \"t\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer5) {
  std::string begin_transaction_body("{ \"v\": \"1.2\", \"t\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer6) {
  std::string begin_transaction_body("{ \"v\": \"1.0\", \"t\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer7) {
  std::string begin_transaction_body("{ \"v\": \"1.0\", \"t\": [] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            SuccessExecutionResult());
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer8) {
  std::string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"blah\": \"12\" }] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer9) {
  std::string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"key\": \"3d4sd\", \"token\": \"ds1\", "
      "\"reporting_time\": \"ffjddjsd123\" }] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer10) {
  std::string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"key\": \"test_key\", \"token\": \"10\" }] "
      "}");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST(ParseBeginTransactionTest, ParseBeginTransactionValidBuffer) {
  std::string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"key\": \"test_key\", \"token\": 10, "
      "\"reporting_time\": \"2021-12-12T17:20:50.52Z\" }, { \"key\": "
      "\"test_key_2\", "
      "\"token\": 23, \"reporting_time\": \"2019-12-12T07:20:50.52Z\" }] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            SuccessExecutionResult());

  EXPECT_EQ(consume_budget_metadata_list.size(), 2);
  auto it = consume_budget_metadata_list.begin();
  EXPECT_EQ(*it->budget_key_name,
            absl::StrCat(kTransactionOriginWithSubdomain, "/test_key"));
  EXPECT_EQ(it->token_count, 10);
  EXPECT_EQ(it->time_bucket, 1639329650000000000);
  ++it;
  EXPECT_EQ(*it->budget_key_name,
            absl::StrCat(kTransactionOriginWithSubdomain, "/test_key_2"));
  EXPECT_EQ(it->token_count, 23);
  EXPECT_EQ(it->time_bucket, 1576135250000000000);
}

TEST(ParseBeginTransactionTest,
     ParseBeginTransactionValidBufferButRepeatedKeysWithinDifferentHours) {
  std::string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"key\": \"test_key\", \"token\": 10, "
      "\"reporting_time\": \"2021-12-12T17:20:50.52Z\" }, { \"key\": "
      "\"test_key\", "
      "\"token\": 23, \"reporting_time\": \"2021-12-12T18:00:00.00Z\" }] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                std::string(kAuthorizedDomain),
                std::string(kTransactionOriginWithSubdomain), bytes_buffer,
                consume_budget_metadata_list),
            SuccessExecutionResult());

  EXPECT_EQ(consume_budget_metadata_list.size(), 2);
  auto it = consume_budget_metadata_list.begin();
  EXPECT_EQ(*it->budget_key_name,
            absl::StrCat(kTransactionOriginWithSubdomain, "/test_key"));
  EXPECT_EQ(it->token_count, 10);
  EXPECT_EQ(it->time_bucket, 1639329650000000000);
  ++it;
  EXPECT_EQ(*it->budget_key_name,
            absl::StrCat(kTransactionOriginWithSubdomain, "/test_key"));
  EXPECT_EQ(it->token_count, 23);
  EXPECT_EQ(it->time_bucket, 1639332000000000000);
}

TEST(ParseBeginTransactionTest,
     ParseBeginTransactionValidBufferButRepeatedKeys) {
  std::string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"key\": \"test_key\", \"token\": 10, "
      "\"reporting_time\": \"2021-12-12T17:20:50.52Z\" }, { \"key\": "
      "\"test_key\", "
      "\"token\": 23, \"reporting_time\": \"2021-12-12T17:20:50.52Z\" }] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(
      ParseBeginTransactionRequestBody(
          std::string(kAuthorizedDomain),
          std::string(kTransactionOriginWithSubdomain), bytes_buffer,
          consume_budget_metadata_list),
      FailureExecutionResult(
          google::scp::core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST));
}

TEST(ParseBeginTransactionTest,
     ParseBeginTransactionValidBufferButRepeatedKeysWithinSameHour) {
  std::string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"key\": \"test_key\", \"token\": 10, "
      "\"reporting_time\": \"2021-12-12T17:20:50.52Z\" }, { \"key\": "
      "\"test_key\", "
      "\"token\": 23, \"reporting_time\": \"2021-12-12T17:59:50.52Z\" }] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = std::make_shared<std::vector<Byte>>(
      begin_transaction_body.begin(), begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::vector<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(
      ParseBeginTransactionRequestBody(
          std::string(kAuthorizedDomain),
          std::string(kTransactionOriginWithSubdomain), bytes_buffer,
          consume_budget_metadata_list),
      FailureExecutionResult(
          google::scp::core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST));
}

TEST(FrontEndUtilsTest, ExtractTransactionId) {
  auto headers = std::make_shared<HttpHeaders>();
  Uuid transaction_id;
  EXPECT_EQ(ExtractTransactionIdFromHTTPHeaders(headers, transaction_id),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));

  headers->insert(
      {std::string(kTransactionIdHeader), std::string("Asdasdasd")});
  EXPECT_EQ(ExtractTransactionIdFromHTTPHeaders(headers, transaction_id),
            FailureExecutionResult(
                ::privacy_sandbox::pbs_common::SC_UUID_INVALID_STRING));

  headers->clear();
  headers->insert({std::string(kTransactionIdHeader),
                   std::string("3E2A3D09-48ED-A355-D346-AD7DC6CB0909")});
  EXPECT_EQ(ExtractTransactionIdFromHTTPHeaders(headers, transaction_id),
            SuccessExecutionResult());
}

TEST(FrontEndUtilsTest, ExtractTransactionOrigin) {
  HttpHeaders headers{};
  auto transaction_origin = ExtractTransactionOrigin(headers);
  EXPECT_EQ(transaction_origin.result(),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));

  std::string extracted_transaction_origin;
  headers.insert({std::string(kTransactionOriginHeader),
                  std::string("This is the origin")});
  transaction_origin = ExtractTransactionOrigin(headers);
  EXPECT_SUCCESS(transaction_origin.result());

  EXPECT_EQ(*transaction_origin, std::string("This is the origin"));
}

TEST(FrontEndUtilsTest, ExtractRequestClaimedIdentity) {
  std::shared_ptr<HttpHeaders> headers = nullptr;
  std::string claimed_identity;
  EXPECT_EQ(ExtractRequestClaimedIdentity(headers, claimed_identity),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));
  headers = std::make_shared<HttpHeaders>();
  EXPECT_EQ(ExtractRequestClaimedIdentity(headers, claimed_identity),
            FailureExecutionResult(
                google::scp::core::errors::
                    SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));

  std::string extracted_claimed_identity;
  headers->insert(
      {std::string(kClaimedIdentityHeader), std::string("other-coordinator")});
  EXPECT_EQ(ExtractRequestClaimedIdentity(headers, extracted_claimed_identity),
            SuccessExecutionResult());

  EXPECT_EQ(extracted_claimed_identity, std::string("other-coordinator"));
}

TEST(FrontEndUtilsTest, SerializeTransactionEmptyFailedCommandIndicesResponse) {
  std::vector<size_t> failed_indices;
  BytesBuffer bytes_buffer;

  EXPECT_EQ(SerializeTransactionFailedCommandIndicesResponse(
                failed_indices, false, bytes_buffer),
            SuccessExecutionResult());

  std::string serialized_failed_response(bytes_buffer.bytes->begin(),
                                         bytes_buffer.bytes->end());
  EXPECT_EQ(serialized_failed_response, "{\"f\":[],\"v\":\"1.0\"}");
  EXPECT_EQ(bytes_buffer.capacity, bytes_buffer.bytes->size());
  EXPECT_EQ(bytes_buffer.length, bytes_buffer.bytes->size());

  EXPECT_EQ(SerializeTransactionFailedCommandIndicesResponse(
                failed_indices, true, bytes_buffer),
            SuccessExecutionResult());

  serialized_failed_response =
      std::string(bytes_buffer.bytes->begin(), bytes_buffer.bytes->end());
  EXPECT_EQ(nlohmann::json::parse(serialized_failed_response),
            nlohmann::json::parse("{\"f\":[],\"v\":\"1.0\"}"));
  EXPECT_EQ(bytes_buffer.capacity, bytes_buffer.bytes->size());
  EXPECT_EQ(bytes_buffer.length, bytes_buffer.bytes->size());
}

TEST(FrontEndUtilsTest, SerializeTransactionFailedCommandIndicesResponse) {
  std::vector<size_t> failed_indices = {1, 2, 3, 4, 5};
  BytesBuffer bytes_buffer;

  EXPECT_EQ(SerializeTransactionFailedCommandIndicesResponse(
                failed_indices, false, bytes_buffer),
            SuccessExecutionResult());

  std::string serialized_failed_response(bytes_buffer.bytes->begin(),
                                         bytes_buffer.bytes->end());
  EXPECT_EQ(serialized_failed_response, "{\"f\":[1,2,3,4,5],\"v\":\"1.0\"}");
  EXPECT_EQ(bytes_buffer.capacity, bytes_buffer.bytes->size());
  EXPECT_EQ(bytes_buffer.length, bytes_buffer.bytes->size());

  EXPECT_EQ(SerializeTransactionFailedCommandIndicesResponse(
                failed_indices, true, bytes_buffer),
            SuccessExecutionResult());

  serialized_failed_response =
      std::string(bytes_buffer.bytes->begin(), bytes_buffer.bytes->end());
  EXPECT_EQ(nlohmann::json::parse(serialized_failed_response),
            nlohmann::json::parse("{\"f\":[1,2,3,4,5],\"v\":\"1.0\"}"));
  EXPECT_EQ(bytes_buffer.capacity, bytes_buffer.bytes->size());
  EXPECT_EQ(bytes_buffer.length, bytes_buffer.bytes->size());
}

TEST(TransformReportingOriginToSite, Success) {
  auto site = TransformReportingOriginToSite("https://analytics.google.com");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpReportingOriginSuccess) {
  auto site = TransformReportingOriginToSite("http://analytics.google.com");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpReportingOriginWithPortSuccess) {
  auto site =
      TransformReportingOriginToSite("http://analytics.google.com:8080");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpReportingOriginWithSlashSuccess) {
  auto site = TransformReportingOriginToSite("http://analytics.google.com/");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite,
     HttpReportingOriginWithPortAndSlashSuccess) {
  auto site =
      TransformReportingOriginToSite("http://analytics.google.com:8080/");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, WithoutHttpsSuccess) {
  auto site = TransformReportingOriginToSite("analytics.google.com");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, WithoutHttpsWithPortSuccess) {
  auto site = TransformReportingOriginToSite("analytics.google.com:8080");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, WithoutHttpsWithSlashSuccess) {
  auto site = TransformReportingOriginToSite("analytics.google.com/");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, WithoutHttpsWithPortAndSlashSuccess) {
  auto site = TransformReportingOriginToSite("analytics.google.com:8080/");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, SiteToSiteSuccess) {
  auto site = TransformReportingOriginToSite("https://google.com");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, SiteWithPortToSiteSuccess) {
  auto site = TransformReportingOriginToSite("https://google.com:8080");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, SiteWithSlashToSiteSuccess) {
  auto site = TransformReportingOriginToSite("https://google.com/");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, SiteWithPortAndSlashToSiteSuccess) {
  auto site = TransformReportingOriginToSite("https://google.com:8080/");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpSiteToSiteSuccess) {
  auto site = TransformReportingOriginToSite("http://google.com");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpSiteWithPortToSiteSuccess) {
  auto site = TransformReportingOriginToSite("http://google.com:8080");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpSiteWithSlashToSiteSuccess) {
  auto site = TransformReportingOriginToSite("http://google.com/");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, HttpSiteWithPortAndSlashToSiteSuccess) {
  auto site = TransformReportingOriginToSite("http://google.com:8080/");
  EXPECT_SUCCESS(site.result());
  EXPECT_EQ(*site, "https://google.com");
}

TEST(TransformReportingOriginToSite, InvalidSite) {
  auto site = TransformReportingOriginToSite("******");
  EXPECT_THAT(site.result(),
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REPORTING_ORIGIN)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, ValidRequestSuccess) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        },
        {
          "key": "234",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "key": "234",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }
      ]
    }
  ]
})");

  MockKeyBodyProcessor mock_processor;

  // Expected arguments for the first call.
  nlohmann::json expected_key_body1 = nlohmann::json::parse(R"({
           "key": "123",
           "token": 1,
           "reporting_time": "2019-12-11T07:20:50.52Z"
         })");
  size_t expected_key_index1 = 0;
  std::string expected_reporting_origin1 = "http://a.fake.com";

  // Expected arguments for the second call.
  nlohmann::json expected_key_body2 = nlohmann::json::parse(R"({
           "key": "234",
           "token": 1,
           "reporting_time": "2019-12-11T07:20:50.52Z"
         })");
  size_t expected_key_index2 = 1;
  std::string expected_reporting_origin2 = "http://a.fake.com";

  // Expected arguments for the third call.
  nlohmann::json expected_key_body3 = nlohmann::json::parse(R"({
           "key": "234",
           "token": 1,
           "reporting_time": "2019-12-12T07:20:50.52Z"
         })");
  size_t expected_key_index3 = 2;
  std::string expected_reporting_origin3 = "http://b.fake.com";

  // Set up the expectations.
  EXPECT_CALL(mock_processor,
              ProcessKeyBody(Eq(expected_key_body1), Eq(expected_key_index1),
                             Eq(expected_reporting_origin1),
                             Eq(kBudgetTypeBinaryBudget)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(mock_processor,
              ProcessKeyBody(Eq(expected_key_body2), Eq(expected_key_index2),
                             Eq(expected_reporting_origin2),
                             Eq(kBudgetTypeBinaryBudget)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(mock_processor,
              ProcessKeyBody(Eq(expected_key_body3), Eq(expected_key_index3),
                             Eq(expected_reporting_origin3),
                             Eq(kBudgetTypeBinaryBudget)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  // Act
  ExecutionResult result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });
  EXPECT_SUCCESS(result);

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;

  PrivacyBudgetKey expected_key_proto1;
  EXPECT_THAT(
      JsonStringToMessage(expected_key_body1.dump(), &expected_key_proto1),
      IsOk());
  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto1),
                                             Eq(expected_key_index1),
                                             Eq(expected_reporting_origin1)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  PrivacyBudgetKey expected_key_proto2;
  EXPECT_THAT(
      JsonStringToMessage(expected_key_body2.dump(), &expected_key_proto2),
      IsOk());
  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto2),
                                             Eq(expected_key_index2),
                                             Eq(expected_reporting_origin2)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  PrivacyBudgetKey expected_key_proto3;
  EXPECT_THAT(
      JsonStringToMessage(expected_key_body3.dump(), &expected_key_proto3),
      IsOk());
  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto3),
                                             Eq(expected_key_index3),
                                             Eq(expected_reporting_origin3)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  ExecutionResult result_proto = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });
  EXPECT_SUCCESS(result_proto);
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithoutVersion) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
     "data": []
   })");

  MockKeyBodyProcessor mock_processor;
  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestEmptyJson) {
  nlohmann::json request_body = nlohmann::json::parse(R"({})");

  MockKeyBodyProcessor mock_processor;
  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithInvalidVersion) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
     "v": "5.0"
   })");

  MockKeyBodyProcessor mock_processor;
  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithoutData) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
     "v": "2.0"
   })");

  MockKeyBodyProcessor mock_processor;
  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  // In proto we cannot distinguish between the data key is absent or has a
  // default value. So, we expect success here.
  EXPECT_SUCCESS(execution_result);
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithInvalidData) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
     "v": "2.0",
     "data": ""
   })");

  MockKeyBodyProcessor mock_processor;
  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithoutReportingOrigin) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }
      ]
    }
  ]
})");

  MockKeyBodyProcessor mock_processor;

  // Expected arguments for the first call.
  nlohmann::json expected_key_body1 = nlohmann::json::parse(R"({
           "key": "123",
           "token": 1,
           "reporting_time": "2019-12-11T07:20:50.52Z"
         })");
  size_t expected_key_index1 = 0;
  std::string expected_reporting_origin1 = "http://a.fake.com";

  // Set up the expectations.
  EXPECT_CALL(mock_processor,
              ProcessKeyBody(Eq(expected_key_body1), Eq(expected_key_index1),
                             Eq(expected_reporting_origin1),
                             Eq(kBudgetTypeBinaryBudget)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  PrivacyBudgetKey expected_key_proto1;
  EXPECT_THAT(
      JsonStringToMessage(expected_key_body1.dump(), &expected_key_proto1),
      IsOk());
  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto1),
                                             Eq(expected_key_index1),
                                             Eq(expected_reporting_origin1)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithoutKeys) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [{
        "key": "123",
        "token": 1,
        "reporting_time": "2019-12-11T07:20:50.52Z"
      }]
    },
    {
      "reporting_origin": "http://b.fake.com"
    }
  ]
})");

  MockKeyBodyProcessor mock_processor;

  // Expected arguments for the first call.
  nlohmann::json expected_key_body1 = nlohmann::json::parse(R"({
           "key": "123",
           "token": 1,
           "reporting_time": "2019-12-11T07:20:50.52Z"
         })");
  size_t expected_key_index1 = 0;
  std::string expected_reporting_origin1 = "http://a.fake.com";

  // Set up the expectations.
  EXPECT_CALL(mock_processor,
              ProcessKeyBody(Eq(expected_key_body1), Eq(expected_key_index1),
                             Eq(expected_reporting_origin1),
                             Eq(kBudgetTypeBinaryBudget)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  PrivacyBudgetKey expected_key_proto1;
  EXPECT_THAT(
      JsonStringToMessage(expected_key_body1.dump(), &expected_key_proto1),
      IsOk());
  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto1),
                                             Eq(expected_key_index1),
                                             Eq(expected_reporting_origin1)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  // In proto we cannot distinguish between the "keys" key is absent or has a
  // default value. So, we expect success here.
  EXPECT_THAT(execution_result, ResultIs(SuccessExecutionResult()));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithEmptyReportingOrigin) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    }
  ]
})");

  MockKeyBodyProcessor mock_processor;
  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest,
     RequestWithInvalidReportingOrigin) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "invalid",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    }
  ]
})");

  MockKeyBodyProcessor mock_processor;
  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest,
     RequestWithUnauthorizedReportingOrigin) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "reporting_origin": "http://b.shoe.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }
      ]
    }
  ]
})");

  MockKeyBodyProcessor mock_processor;

  // Expected arguments for the first call.
  nlohmann::json expected_key_body1 = nlohmann::json::parse(R"({
           "key": "123",
           "token": 1,
           "reporting_time": "2019-12-11T07:20:50.52Z"
         })");
  size_t expected_key_index1 = 0;
  std::string expected_reporting_origin1 = "http://a.fake.com";

  // Set up the expectations.
  EXPECT_CALL(mock_processor,
              ProcessKeyBody(Eq(expected_key_body1), Eq(expected_key_index1),
                             Eq(expected_reporting_origin1),
                             Eq(kBudgetTypeBinaryBudget)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_THAT(
      execution_result,
      ResultIs(FailureExecutionResult(
          google::scp::core::errors::
              SC_PBS_FRONT_END_SERVICE_REPORTING_ORIGIN_NOT_BELONG_TO_SITE)));

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  PrivacyBudgetKey expected_key_proto1;
  EXPECT_THAT(
      JsonStringToMessage(expected_key_body1.dump(), &expected_key_proto1),
      IsOk());
  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto1),
                                             Eq(expected_key_index1),
                                             Eq(expected_reporting_origin1)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(
      execution_result,
      ResultIs(FailureExecutionResult(
          google::scp::core::errors::
              SC_PBS_FRONT_END_SERVICE_REPORTING_ORIGIN_NOT_BELONG_TO_SITE)));
}

TEST(ParseCommonV2TransactionRequestBodyTest,
     RequestWithRepeatedReportingOrigin) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }
      ]
    }
  ]
})");

  MockKeyBodyProcessor mock_processor;

  // Expected arguments for the first call.
  nlohmann::json expected_key_body1 = nlohmann::json::parse(R"({
     "key": "123",
     "token": 1,
     "reporting_time": "2019-12-11T07:20:50.52Z"
   })");
  size_t expected_key_index1 = 0;
  std::string expected_reporting_origin1 = "http://a.fake.com";

  // Set up the expectations.
  EXPECT_CALL(mock_processor,
              ProcessKeyBody(Eq(expected_key_body1), Eq(expected_key_index1),
                             Eq(expected_reporting_origin1),
                             Eq(kBudgetTypeBinaryBudget)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  PrivacyBudgetKey expected_key_proto1;
  EXPECT_THAT(
      JsonStringToMessage(expected_key_body1.dump(), &expected_key_proto1),
      IsOk());
  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto1),
                                             Eq(expected_key_index1),
                                             Eq(expected_reporting_origin1)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithBudgetTypeSpecified) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z",
          "budget_type": "local_pbs"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z",
          "budget_type": "local_pbs"
        }
      ]
    }
  ]
})");

  MockKeyBodyProcessor mock_processor;

  // Expected arguments for the first call.
  nlohmann::json expected_key_body1 = nlohmann::json::parse(R"({
     "key": "123",
     "token": 1,
     "reporting_time": "2019-12-11T07:20:50.52Z",
     "budget_type": "local_pbs"
   })");
  size_t expected_key_index1 = 0;
  std::string expected_reporting_origin1 = "http://a.fake.com";

  // Expected arguments for the second call.
  nlohmann::json expected_key_body2 = nlohmann::json::parse(R"({
       "key": "456",
       "token": 1,
       "reporting_time": "2019-12-12T07:20:50.52Z",
       "budget_type": "local_pbs"
   })");
  size_t expected_key_index2 = 1;
  std::string expected_reporting_origin2 = "http://b.fake.com";

  // Set up the expectations.
  EXPECT_CALL(mock_processor,
              ProcessKeyBody(Eq(expected_key_body1), Eq(expected_key_index1),
                             Eq(expected_reporting_origin1), Eq("local_pbs")))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  EXPECT_CALL(mock_processor,
              ProcessKeyBody(Eq(expected_key_body2), Eq(expected_key_index2),
                             Eq(expected_reporting_origin2), Eq("local_pbs")))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_SUCCESS(execution_result);

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  request_body["data"][0]["keys"][0]["budget_type"] = kBudgetTypeBinaryBudget;
  request_body["data"][1]["keys"][0]["budget_type"] = kBudgetTypeBinaryBudget;
  expected_key_body1["budget_type"] = kBudgetTypeBinaryBudget;
  expected_key_body2["budget_type"] = kBudgetTypeBinaryBudget;

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  PrivacyBudgetKey expected_key_proto1;
  EXPECT_THAT(
      JsonStringToMessage(expected_key_body1.dump(), &expected_key_proto1),
      IsOk());
  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto1),
                                             Eq(expected_key_index1),
                                             Eq(expected_reporting_origin1)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  PrivacyBudgetKey expected_key_proto2;
  EXPECT_THAT(
      JsonStringToMessage(expected_key_body2.dump(), &expected_key_proto2),
      IsOk());
  EXPECT_CALL(mock_processor, ProcessKeyBody(EqualsProto(expected_key_proto2),
                                             Eq(expected_key_index2),
                                             Eq(expected_reporting_origin2)))
      .Times(1)
      .WillOnce(Return(SuccessExecutionResult()));

  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });
  EXPECT_SUCCESS(execution_result);
}

TEST(ParseCommonV2TransactionRequestBodyTest,
     RequestWithDifferentBudgetTypeSpecified) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z",
          "budget_type": "type1"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z",
          "budget_type": "type2"
        }
      ]
    }
  ]
})");

  MockKeyBodyProcessor mock_processor;
  // Set up the expectations.
  EXPECT_CALL(mock_processor,
              ProcessKeyBody(_, _, _, AnyOf(Eq("type1"), Eq("type2"))))
      .Times(2)
      .WillRepeatedly(Return(SuccessExecutionResult()));

  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });
  EXPECT_SUCCESS(execution_result);
}

TEST(ParseCommonV2TransactionRequestBodyTest,
     RequestWithBudgetTypeNotSpecifiedInSecondKey) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z",
          "budget_type": "type1"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }
      ]
    }
  ]
})");

  MockKeyBodyProcessor mock_processor;
  // Set up the expectations.
  EXPECT_CALL(
      mock_processor,
      ProcessKeyBody(_, _, _, AnyOf(Eq("type1"), Eq(kBudgetTypeBinaryBudget))))
      .Times(2)
      .WillRepeatedly(Return(SuccessExecutionResult()));

  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });
  EXPECT_SUCCESS(execution_result);
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithEmptyBudgetType) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z",
          "budget_type": ""
        }
      ]
    }
  ]
})");

  MockKeyBodyProcessor mock_processor;
  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST(ParseCommonV2TransactionRequestBodyTest, RequestWithNoData) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
     "v": "2.0",
     "data": []
   })");

  MockKeyBodyProcessor mock_processor;

  EXPECT_CALL(mock_processor, ProcessKeyBody(_, _, _, _)).Times(0);

  ExecutionResult execution_result = ParseCommonV2TransactionRequestBody(
      kAuthorizedDomain, request_body,
      [&](const nlohmann::json& key_body, size_t key_index,
          absl::string_view reporting_origin, absl::string_view budget_type) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin, budget_type);
      });

  EXPECT_SUCCESS(execution_result);

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());

  execution_result = ParseCommonV2TransactionRequestProto(
      kAuthorizedDomain, request_proto,
      [&](const PrivacyBudgetKey& key_body, size_t key_index,
          absl::string_view reporting_origin) {
        return mock_processor.ProcessKeyBody(key_body, key_index,
                                             reporting_origin);
      });

  EXPECT_SUCCESS(execution_result);
}

TEST(CheckAndGetIfBudgetTypeTheSameInRequestTest,
     RequestWithNoBudgetTypeSpecified) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }
      ]
    }
  ]
})");

  auto execution_result = ValidateAndGetBudgetType(request_body);
  EXPECT_SUCCESS(execution_result);
  EXPECT_EQ(*execution_result, kBudgetTypeBinaryBudget);

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());
  auto execution_result_proto = ValidateAndGetBudgetType(request_proto);
  EXPECT_EQ(
      *execution_result_proto,
      ConsumePrivacyBudgetRequest::PrivacyBudgetKey::BUDGET_TYPE_BINARY_BUDGET);
}

TEST(CheckAndGetIfBudgetTypeTheSameInRequestTest,
     RequestWithSameBudgetTypeSpecified) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z",
          "budget_type": "BUDGET_TYPE_BINARY_BUDGET"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z",
          "budget_type": "BUDGET_TYPE_BINARY_BUDGET"
        }
      ]
    }
  ]
})");

  auto execution_result = ValidateAndGetBudgetType(request_body);
  EXPECT_SUCCESS(execution_result);
  EXPECT_EQ(*execution_result, "BUDGET_TYPE_BINARY_BUDGET");

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());
  auto execution_result_proto = ValidateAndGetBudgetType(request_proto);
  EXPECT_EQ(
      *execution_result_proto,
      ConsumePrivacyBudgetRequest::PrivacyBudgetKey::BUDGET_TYPE_BINARY_BUDGET);
}

TEST(CheckAndGetIfBudgetTypeTheSameInRequestTest, V1Request) {
  nlohmann::json request_body =
      nlohmann::json::parse(R"({ "v": "1.0", "t": [] })");

  auto execution_result = ValidateAndGetBudgetType(request_body);
  EXPECT_SUCCESS(execution_result);
  EXPECT_EQ(*execution_result, kBudgetTypeBinaryBudget);
}

TEST(CheckAndGetIfBudgetTypeTheSameInRequestTest,
     RequestWithDifferentBudgetTypeSpecified) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z",
          "budget_type": "type1"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z",
          "budget_type": "type2"
        }
      ]
    }
  ]
})");

  auto execution_result = ValidateAndGetBudgetType(request_body);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  request_body["data"][0]["keys"][0]["budget_type"] =
      PrivacyBudgetKey::BudgetType_Name(
          PrivacyBudgetKey::BUDGET_TYPE_UNSPECIFIED);
  request_body["data"][1]["keys"][0]["budget_type"] = kBudgetTypeBinaryBudget;

  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());
  auto execution_result_proto = ValidateAndGetBudgetType(request_proto);
  EXPECT_SUCCESS(execution_result_proto);
  EXPECT_EQ(
      *execution_result_proto,
      ConsumePrivacyBudgetRequest::PrivacyBudgetKey::BUDGET_TYPE_BINARY_BUDGET);

  // This an attempt to introduce fake budget types since the only
  // available budget types available at this time are BUDGET_TYPE_UNSPECIFIED
  // and BUDGET_TYPE_BINARY_BUDGET which are equivalent
  PrivacyBudgetKey::BudgetType fake_budget_type1 =
      static_cast<typename PrivacyBudgetKey::BudgetType>(1000);
  PrivacyBudgetKey::BudgetType fake_budget_type2 =
      static_cast<typename PrivacyBudgetKey::BudgetType>(1001);
  request_proto.mutable_data(0)->mutable_keys(0)->set_budget_type(
      fake_budget_type1);
  request_proto.mutable_data(1)->mutable_keys(0)->set_budget_type(
      fake_budget_type2);

  execution_result_proto = ValidateAndGetBudgetType(request_proto);
  EXPECT_THAT(execution_result_proto,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
}

TEST(CheckAndGetIfBudgetTypeTheSameInRequestTest,
     RequestWithBudgetTypeNotSpecifiedInSecondKey) {
  nlohmann::json request_body = nlohmann::json::parse(R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "http://a.fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z",
          "budget_type": "type1"
        }
      ]
    },
    {
      "reporting_origin": "http://b.fake.com",
      "keys": [
        {
          "key": "456",
          "token": 1,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }
      ]
    }
  ]
})");

  auto execution_result = ValidateAndGetBudgetType(request_body);
  EXPECT_THAT(execution_result,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;

  request_body["data"][0]["keys"][0]["budget_type"] = kBudgetTypeBinaryBudget;
  ConsumePrivacyBudgetRequest request_proto;
  EXPECT_THAT(JsonStringToMessage(request_body.dump(), &request_proto), IsOk());
  auto execution_result_proto = ValidateAndGetBudgetType(request_proto);
  EXPECT_EQ(*execution_result_proto,
            PrivacyBudgetKey::BUDGET_TYPE_BINARY_BUDGET);

  // This an attempt to introduce fake budget types since the only
  // available budget types available at this time are BUDGET_TYPE_UNSPECIFIED
  // and BUDGET_TYPE_BINARY_BUDGET which are equivalent
  PrivacyBudgetKey::BudgetType fake_budget_type =
      static_cast<typename PrivacyBudgetKey::BudgetType>(1000);
  request_proto.mutable_data(0)->mutable_keys(0)->set_budget_type(
      fake_budget_type);

  execution_result_proto = ValidateAndGetBudgetType(request_proto);
  EXPECT_THAT(execution_result_proto,
              ResultIs(FailureExecutionResult(
                  google::scp::core::errors::
                      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
}

}  // namespace
}  // namespace google::scp::pbs
