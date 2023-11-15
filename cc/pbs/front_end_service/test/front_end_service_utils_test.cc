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

#include <gtest/gtest.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "core/common/uuid/src/error_codes.h"
#include "pbs/front_end_service/src/error_codes.h"
#include "pbs/front_end_service/src/front_end_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

namespace google::scp::pbs::test {

using ::google::scp::core::Byte;
using ::google::scp::core::BytesBuffer;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::GetTransactionStatusResponse;
using ::google::scp::core::HttpHeaders;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::TransactionExecutionPhase;
using ::google::scp::core::common::Uuid;
using ::google::scp::core::test::ResultIs;
using ::google::scp::pbs::FrontEndUtils;
using ::std::list;
using ::std::make_shared;
using ::std::shared_ptr;
using ::std::string;
using ::std::vector;

static constexpr char kAuthorizedDomain[] = "http://fake-reporting-origin.com";

struct ParseBeginTransactionTestCase {
  std::string test_name;
  bool enable_per_site_enrollment;
};

class ParseBeginTransactionTest
    : public testing::TestWithParam<ParseBeginTransactionTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ParseBeginTransactionTest, ParseBeginTransactionTest,
    testing::Values(
        ParseBeginTransactionTestCase{"EnablePerSiteEnrollment", true},
        ParseBeginTransactionTestCase{"DisablePerSiteEnrollment", false}),
    [](const testing::TestParamInfo<ParseBeginTransactionTest::ParamType>&
           info) { return info.param.test_name; });

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionV2RequestSucess) {
  std::string begin_transaction_body = R"({
    "v": "2.0",
    "data": [
      {
        "reporting_origin": "http://a.com",
        "keys": [{
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }]
      },
      {
        "reporting_origin": "http://b.com",
        "keys": [{
          "key": "456",
          "token": 2,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }]
      }
    ]
  })";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
      GetParam().enable_per_site_enrollment);
  if (GetParam().enable_per_site_enrollment) {
    EXPECT_SUCCESS(execution_result);
    EXPECT_EQ(consume_budget_metadata_list.size(), 2);

    auto it = consume_budget_metadata_list.begin();
    EXPECT_EQ(*it->budget_key_name, "http://a.com/123");
    EXPECT_EQ(it->token_count, 1);
    EXPECT_EQ(it->time_bucket, 1576048850000000000);

    ++it;
    EXPECT_EQ(*it->budget_key_name, "http://b.com/456");
    EXPECT_EQ(it->token_count, 2);
    EXPECT_EQ(it->time_bucket, 1576135250000000000);
  } else {
    EXPECT_THAT(
        execution_result,
        ResultIs(FailureExecutionResult(
            core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
  }
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionV2RequestWithoutData) {
  std::string begin_transaction_body = R"({
    "v": "2.0",
  })";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
      GetParam().enable_per_site_enrollment);
  EXPECT_THAT(
      execution_result,
      ResultIs(FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionV2RequestInvalidJson) {
  std::string begin_transaction_body = R"({
    "invalid"
  })";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
      GetParam().enable_per_site_enrollment);
  EXPECT_THAT(
      execution_result,
      ResultIs(FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(ParseBeginTransactionTest,
       ParseBeginTransactionV2RequestWithoutReportingOrigin) {
  std::string begin_transaction_body = R"({
    "v": "2.0",
    "data": [
      {
        "reporting_origin": "http://a.com",
        "keys": [{
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }]
      },
      {
        "keys": [{
          "key": "456",
          "token": 2,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }]
      }
    ]
  })";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
      GetParam().enable_per_site_enrollment);
  EXPECT_THAT(
      execution_result,
      ResultIs(FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionV2RequestWithoutKeys) {
  std::string begin_transaction_body = R"({
    "v": "2.0",
    "data": [
      {
        "reporting_origin": "http://a.com",
        "keys": [{
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }]
      },
      {
        "reporting_origin": "http://b.com",
      }
    ]
  })";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
      GetParam().enable_per_site_enrollment);
  EXPECT_THAT(
      execution_result,
      ResultIs(FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(ParseBeginTransactionTest,
       ParseBeginTransactionV2RequestWithTwoEqualsReportingOrigin) {
  std::string begin_transaction_body = R"({
    "v": "2.0",
    "data": [
      {
        "reporting_origin": "http://a.com",
        "keys": [{
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }]
      },
      {
        "reporting_origin": "http://a.com",
        "keys": [{
          "key": "456",
          "token": 2,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }]
      }
    ]
  })";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
      GetParam().enable_per_site_enrollment);
  if (GetParam().enable_per_site_enrollment) {
    EXPECT_THAT(execution_result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
  } else {
    EXPECT_THAT(
        execution_result,
        ResultIs(FailureExecutionResult(
            core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
  }
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionV2RequestWithoutKey) {
  std::string begin_transaction_body = R"({
    "v": "2.0",
    "data": [
      {
        "reporting_origin": "http://a.com",
        "keys": [{
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }]
      },
      {
        "reporting_origin": "http://b.com",
        "keys": [{
          "token": 2,
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }]
      }
    ]
  })";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
      GetParam().enable_per_site_enrollment);
  EXPECT_THAT(
      execution_result,
      ResultIs(FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionV2RequestWithoutToken) {
  std::string begin_transaction_body = R"({
    "v": "2.0",
    "data": [
      {
        "reporting_origin": "http://a.com",
        "keys": [{
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }]
      },
      {
        "reporting_origin": "http://b.com",
        "keys": [{
          "key": "456",
          "reporting_time": "2019-12-12T07:20:50.52Z"
        }]
      }
    ]
  })";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
      GetParam().enable_per_site_enrollment);
  EXPECT_THAT(
      execution_result,
      ResultIs(FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(ParseBeginTransactionTest,
       ParseBeginTransactionV2RequestWithoutReportingTime) {
  std::string begin_transaction_body = R"({
    "v": "2.0",
    "data": [
      {
        "reporting_origin": "http://a.com",
        "keys": [{
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }]
      },
      {
        "reporting_origin": "http://b.com",
        "keys": [{
          "key": "456",
          "token": 2,
        }]
      }
    ]
  })";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
      GetParam().enable_per_site_enrollment);
  EXPECT_THAT(
      execution_result,
      ResultIs(FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
}

TEST_P(ParseBeginTransactionTest,
       ParseBeginTransactionV2RequestWithInvalidReportingTime) {
  std::string begin_transaction_body = R"({
    "v": "2.0",
    "data": [
      {
        "reporting_origin": "http://a.com",
        "keys": [{
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z"
        }]
      },
      {
        "reporting_origin": "http://b.com",
        "keys": [{
          "key": "456",
          "token": 2,
          "reporting_time": "invalid"
        }]
      }
    ]
  })";

  BytesBuffer bytes_buffer(begin_transaction_body);

  std::list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
      GetParam().enable_per_site_enrollment);
  if (GetParam().enable_per_site_enrollment) {
    EXPECT_THAT(execution_result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
  } else {
    EXPECT_THAT(
        execution_result,
        ResultIs(FailureExecutionResult(
            core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
  }
}

TEST_P(ParseBeginTransactionTest,
       ParseBeginTransactionV2RequestWithEqualsBudgetKey) {
  std::string begin_transaction_body = R"({
    "v": "2.0",
    "data": [
      {
        "reporting_origin": "http://a.com",
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

  std::list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  auto execution_result = ParseBeginTransactionRequestBody(
      kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
      GetParam().enable_per_site_enrollment);
  if (GetParam().enable_per_site_enrollment) {
    EXPECT_THAT(execution_result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST)));
  } else {
    EXPECT_THAT(
        execution_result,
        ResultIs(FailureExecutionResult(
            core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY)));
  }
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer) {
  BytesBuffer bytes_buffer;
  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer1) {
  BytesBuffer bytes_buffer(120);
  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer2) {
  string begin_transaction_body("{}");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer3) {
  string begin_transaction_body("{ \"v\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer4) {
  string begin_transaction_body("{ \"v\": \"\", \"t\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer5) {
  string begin_transaction_body("{ \"v\": \"1.2\", \"t\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer6) {
  string begin_transaction_body("{ \"v\": \"1.0\", \"t\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer7) {
  string begin_transaction_body("{ \"v\": \"1.0\", \"t\": [] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            SuccessExecutionResult());
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer8) {
  string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"blah\": \"12\" }] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer9) {
  string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"key\": \"3d4sd\", \"token\": \"ds1\", "
      "\"reporting_time\": \"ffjddjsd123\" }] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionInvalidBuffer10) {
  string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"key\": \"test_key\", \"token\": \"10\" }] "
      "}");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY));
}

TEST_P(ParseBeginTransactionTest, ParseBeginTransactionValidBuffer) {
  string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"key\": \"test_key\", \"token\": 10, "
      "\"reporting_time\": \"2021-12-12T17:20:50.52Z\" }, { \"key\": "
      "\"test_key_2\", "
      "\"token\": 23, \"reporting_time\": \"2019-12-12T07:20:50.52Z\" }] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            SuccessExecutionResult());

  EXPECT_EQ(consume_budget_metadata_list.size(), 2);
  auto it = consume_budget_metadata_list.begin();
  if (GetParam().enable_per_site_enrollment) {
    EXPECT_EQ(*it->budget_key_name,
              absl::StrCat(kAuthorizedDomain, "/test_key"));
  } else {
    EXPECT_EQ(*it->budget_key_name, "test_key");
  }
  EXPECT_EQ(it->token_count, 10);
  EXPECT_EQ(it->time_bucket, 1639329650000000000);
  ++it;
  if (GetParam().enable_per_site_enrollment) {
    EXPECT_EQ(*it->budget_key_name,
              absl::StrCat(kAuthorizedDomain, "/test_key_2"));
  } else {
    EXPECT_EQ(*it->budget_key_name, "test_key_2");
  }
  EXPECT_EQ(it->token_count, 23);
  EXPECT_EQ(it->time_bucket, 1576135250000000000);
}

TEST_P(ParseBeginTransactionTest,
       ParseBeginTransactionValidBufferButRepeatedKeysWithinDifferentHours) {
  string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"key\": \"test_key\", \"token\": 10, "
      "\"reporting_time\": \"2021-12-12T17:20:50.52Z\" }, { \"key\": "
      "\"test_key\", "
      "\"token\": 23, \"reporting_time\": \"2021-12-12T18:00:00.00Z\" }] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            SuccessExecutionResult());

  EXPECT_EQ(consume_budget_metadata_list.size(), 2);
  auto it = consume_budget_metadata_list.begin();
  if (GetParam().enable_per_site_enrollment) {
    EXPECT_EQ(*it->budget_key_name,
              absl::StrCat(kAuthorizedDomain, "/test_key"));
  } else {
    EXPECT_EQ(*it->budget_key_name, "test_key");
  }
  EXPECT_EQ(it->token_count, 10);
  EXPECT_EQ(it->time_bucket, 1639329650000000000);
  ++it;
  if (GetParam().enable_per_site_enrollment) {
    EXPECT_EQ(*it->budget_key_name,
              absl::StrCat(kAuthorizedDomain, "/test_key"));
  } else {
    EXPECT_EQ(*it->budget_key_name, "test_key");
  }
  EXPECT_EQ(it->token_count, 23);
  EXPECT_EQ(it->time_bucket, 1639332000000000000);
}

TEST_P(ParseBeginTransactionTest,
       ParseBeginTransactionValidBufferButRepeatedKeys) {
  string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"key\": \"test_key\", \"token\": 10, "
      "\"reporting_time\": \"2021-12-12T17:20:50.52Z\" }, { \"key\": "
      "\"test_key\", "
      "\"token\": 23, \"reporting_time\": \"2021-12-12T17:20:50.52Z\" }] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST));
}

TEST_P(ParseBeginTransactionTest,
       ParseBeginTransactionValidBufferButRepeatedKeysWithinSameHour) {
  string begin_transaction_body(
      "{ \"v\": \"1.0\", \"t\": [{ \"key\": \"test_key\", \"token\": 10, "
      "\"reporting_time\": \"2021-12-12T17:20:50.52Z\" }, { \"key\": "
      "\"test_key\", "
      "\"token\": 23, \"reporting_time\": \"2021-12-12T17:59:50.52Z\" }] }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;

  EXPECT_EQ(ParseBeginTransactionRequestBody(
                kAuthorizedDomain, bytes_buffer, consume_budget_metadata_list,
                GetParam().enable_per_site_enrollment),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST));
}

TEST(FrontEndUtilsTest, ExtractTransactionId) {
  auto headers = make_shared<HttpHeaders>();
  Uuid transaction_id;
  EXPECT_EQ(
      FrontEndUtils::ExtractTransactionId(headers, transaction_id),
      FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));

  headers->insert({string(kTransactionIdHeader), string("Asdasdasd")});
  EXPECT_EQ(FrontEndUtils::ExtractTransactionId(headers, transaction_id),
            FailureExecutionResult(core::errors::SC_UUID_INVALID_STRING));

  headers->clear();
  headers->insert({string(kTransactionIdHeader),
                   string("3E2A3D09-48ED-A355-D346-AD7DC6CB0909")});
  EXPECT_EQ(FrontEndUtils::ExtractTransactionId(headers, transaction_id),
            SuccessExecutionResult());
}

TEST(FrontEndUtilsTest, ExtractTransactionSecret) {
  auto headers = make_shared<HttpHeaders>();
  string transaction_secret;
  EXPECT_EQ(
      FrontEndUtils::ExtractTransactionSecret(headers, transaction_secret),
      FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));

  string extracted_transaction_secret;
  headers->insert({string(kTransactionSecretHeader), string("secret")});
  EXPECT_EQ(FrontEndUtils::ExtractTransactionSecret(
                headers, extracted_transaction_secret),
            SuccessExecutionResult());

  EXPECT_EQ(extracted_transaction_secret, string("secret"));
}

TEST(FrontEndUtilsTest, ExtractTransactionOrigin) {
  auto headers = make_shared<HttpHeaders>();
  string transaction_origin;
  EXPECT_EQ(
      FrontEndUtils::ExtractTransactionOrigin(headers, transaction_origin),
      FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));

  string extracted_transaction_origin;
  headers->insert(
      {string(kTransactionOriginHeader), string("This is the origin")});
  EXPECT_EQ(FrontEndUtils::ExtractTransactionOrigin(
                headers, extracted_transaction_origin),
            SuccessExecutionResult());

  EXPECT_EQ(extracted_transaction_origin, string("This is the origin"));
}

TEST(FrontEndUtilsTest, ExtractRequestClaimedIdentity) {
  shared_ptr<core::HttpHeaders> headers = nullptr;
  string claimed_identity;
  EXPECT_EQ(
      FrontEndUtils::ExtractRequestClaimedIdentity(headers, claimed_identity),
      FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));
  headers = make_shared<HttpHeaders>();
  EXPECT_EQ(
      FrontEndUtils::ExtractRequestClaimedIdentity(headers, claimed_identity),
      FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND));

  string extracted_claimed_identity;
  headers->insert(
      {string(core::kClaimedIdentityHeader), string("other-coordinator")});
  EXPECT_EQ(FrontEndUtils::ExtractRequestClaimedIdentity(
                headers, extracted_claimed_identity),
            SuccessExecutionResult());

  EXPECT_EQ(extracted_claimed_identity, string("other-coordinator"));
}

TEST(FrontEndUtilsTest, IsCoordinatorRequest) {
  auto headers = make_shared<HttpHeaders>();
  string coordinator_claimed_identity = "other-coordinator";
  EXPECT_FALSE(FrontEndUtils::IsCoordinatorRequest(
      headers, coordinator_claimed_identity));

  headers->insert(
      {string(core::kClaimedIdentityHeader), string("other-coordinator")});
  EXPECT_TRUE(FrontEndUtils::IsCoordinatorRequest(
      headers, coordinator_claimed_identity));
}

TEST(FrontEndUtilsTest, DeserializeGetTransactionStatusInvalidBuffer) {
  BytesBuffer bytes_buffer;
  std::shared_ptr<core::GetTransactionStatusResponse>
      get_transaction_status_response;

  EXPECT_EQ(FrontEndUtils::DeserializeGetTransactionStatus(
                bytes_buffer, get_transaction_status_response),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
}

TEST(FrontEndUtilsTest, DeserializeGetTransactionStatusInvalidBuffer1) {
  BytesBuffer bytes_buffer(120);
  std::shared_ptr<core::GetTransactionStatusResponse>
      get_transaction_status_response;

  EXPECT_EQ(FrontEndUtils::DeserializeGetTransactionStatus(
                bytes_buffer, get_transaction_status_response),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
}

TEST(FrontEndUtilsTest, DeserializeGetTransactionStatusInvalidBuffer2) {
  string begin_transaction_body("{}");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::shared_ptr<core::GetTransactionStatusResponse>
      get_transaction_status_response;

  EXPECT_EQ(FrontEndUtils::DeserializeGetTransactionStatus(
                bytes_buffer, get_transaction_status_response),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
}

TEST(FrontEndUtilsTest, DeserializeGetTransactionStatusInvalidBuffer3) {
  string begin_transaction_body("{ \"is_expired\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::shared_ptr<core::GetTransactionStatusResponse>
      get_transaction_status_response;

  EXPECT_EQ(FrontEndUtils::DeserializeGetTransactionStatus(
                bytes_buffer, get_transaction_status_response),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
}

TEST(FrontEndUtilsTest, DeserializeGetTransactionStatusInvalidBuffer4) {
  string begin_transaction_body(
      "{ \"is_expired\": \"\", \"has_failures\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::shared_ptr<core::GetTransactionStatusResponse>
      get_transaction_status_response;

  EXPECT_EQ(FrontEndUtils::DeserializeGetTransactionStatus(
                bytes_buffer, get_transaction_status_response),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
}

TEST(FrontEndUtilsTest, DeserializeGetTransactionStatusInvalidBuffer5) {
  string begin_transaction_body(
      "{ \"is_expired\": \"\", \"has_failures\": \"\", "
      "\"last_execution_timestamp\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::shared_ptr<core::GetTransactionStatusResponse>
      get_transaction_status_response;

  EXPECT_EQ(FrontEndUtils::DeserializeGetTransactionStatus(
                bytes_buffer, get_transaction_status_response),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
}

TEST(FrontEndUtilsTest, DeserializeGetTransactionStatusInvalidBuffer6) {
  string begin_transaction_body(
      "{ \"is_expired\": \"\", \"has_failures\": \"\", "
      "\"last_execution_timestamp\": \"\", \"transaction_execution_phase\": "
      "\"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::shared_ptr<core::GetTransactionStatusResponse>
      get_transaction_status_response;

  EXPECT_EQ(FrontEndUtils::DeserializeGetTransactionStatus(
                bytes_buffer, get_transaction_status_response),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
}

TEST(FrontEndUtilsTest, DeserializeGetTransactionStatusInvalidBuffer7) {
  string begin_transaction_body(
      "{ \"is_expired\": \"\", \"has_failures\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::shared_ptr<core::GetTransactionStatusResponse>
      get_transaction_status_response;

  EXPECT_EQ(FrontEndUtils::DeserializeGetTransactionStatus(
                bytes_buffer, get_transaction_status_response),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
}

TEST(FrontEndUtilsTest, DeserializeGetTransactionStatusInvalidBuffer8) {
  string begin_transaction_body(
      "{ \"is_expired\": \"\", "
      "\"last_execution_timestamp\": \"\", \"transaction_execution_phase\": "
      "\"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::shared_ptr<core::GetTransactionStatusResponse>
      get_transaction_status_response;

  EXPECT_EQ(FrontEndUtils::DeserializeGetTransactionStatus(
                bytes_buffer, get_transaction_status_response),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
}

TEST(FrontEndUtilsTest, DeserializeGetTransactionStatusInvalidBuffer9) {
  string begin_transaction_body(
      "{ \"is_expired\": \"\", \"has_failures\": \"\", "
      "\"last_execution_timestamp\": \"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::shared_ptr<core::GetTransactionStatusResponse>
      get_transaction_status_response;

  EXPECT_EQ(FrontEndUtils::DeserializeGetTransactionStatus(
                bytes_buffer, get_transaction_status_response),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
}

TEST(FrontEndUtilsTest, DeserializeGetTransactionStatusInvalidBuffer10) {
  string begin_transaction_body(
      "{ \"is_expired\": \"\", \"has_failures\": \"\", "
      "\"last_execution_timestamp\": \"\", \"transaction_execution_phase\": "
      "\"\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  std::shared_ptr<core::GetTransactionStatusResponse>
      get_transaction_status_response;

  EXPECT_EQ(FrontEndUtils::DeserializeGetTransactionStatus(
                bytes_buffer, get_transaction_status_response),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
}

TEST(FrontEndUtilsTest, DeserializeGetTransactionStatus) {
  string begin_transaction_body(
      "{ \"is_expired\": true, \"has_failures\": true, "
      "\"last_execution_timestamp\": 12341231, "
      "\"transaction_execution_phase\": \"COMMIT\" }");
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(begin_transaction_body.begin(),
                                                 begin_transaction_body.end());
  bytes_buffer.capacity = begin_transaction_body.length();
  bytes_buffer.length = begin_transaction_body.length();

  shared_ptr<GetTransactionStatusResponse> get_transaction_status_response =
      make_shared<GetTransactionStatusResponse>();

  EXPECT_EQ(FrontEndUtils::DeserializeGetTransactionStatus(
                bytes_buffer, get_transaction_status_response),
            SuccessExecutionResult());
  EXPECT_EQ(get_transaction_status_response->is_expired, true);
  EXPECT_EQ(get_transaction_status_response->has_failure, true);
  EXPECT_EQ(get_transaction_status_response->last_execution_timestamp,
            12341231);
  EXPECT_EQ(get_transaction_status_response->transaction_execution_phase,
            TransactionExecutionPhase::Commit);
}

TEST(FrontEndUtilsTest, SerializeGetTransactionStatus) {
  BytesBuffer bytes_buffer;

  shared_ptr<GetTransactionStatusResponse> get_transaction_status_response =
      make_shared<GetTransactionStatusResponse>();
  get_transaction_status_response->has_failure = true;
  get_transaction_status_response->is_expired = false;
  get_transaction_status_response->last_execution_timestamp = 1234512313;
  get_transaction_status_response->transaction_execution_phase =
      TransactionExecutionPhase::Notify;

  EXPECT_EQ(FrontEndUtils::SerializeGetTransactionStatus(
                get_transaction_status_response, bytes_buffer),
            SuccessExecutionResult());

  string get_transaction_status(
      "{\"has_failures\":true,\"is_expired\":false,\"last_execution_"
      "timestamp\":1234512313,\"transaction_execution_phase\":\"NOTIFY\"}");

  string serialized_get_transaction_status(bytes_buffer.bytes->begin(),
                                           bytes_buffer.bytes->end());
  EXPECT_EQ(get_transaction_status, serialized_get_transaction_status);
}

TEST(FrontEndUtilsTest, SerializeTransactionEmptyFailedCommandIndicesResponse) {
  list<size_t> failed_indices;
  BytesBuffer bytes_buffer;

  EXPECT_EQ(FrontEndUtils::SerializeTransactionFailedCommandIndicesResponse(
                failed_indices, bytes_buffer),
            SuccessExecutionResult());

  string serialized_failed_response(bytes_buffer.bytes->begin(),
                                    bytes_buffer.bytes->end());

  EXPECT_EQ(serialized_failed_response, "{\"f\":[],\"v\":\"1.0\"}");
  EXPECT_EQ(bytes_buffer.capacity, bytes_buffer.bytes->size());
  EXPECT_EQ(bytes_buffer.length, bytes_buffer.bytes->size());
}

TEST(FrontEndUtilsTest, SerializeTransactionFailedCommandIndicesResponse) {
  list<size_t> failed_indices = {1, 2, 3, 4, 5};
  BytesBuffer bytes_buffer;

  EXPECT_EQ(FrontEndUtils::SerializeTransactionFailedCommandIndicesResponse(
                failed_indices, bytes_buffer),
            SuccessExecutionResult());

  string serialized_failed_response(bytes_buffer.bytes->begin(),
                                    bytes_buffer.bytes->end());

  EXPECT_EQ(serialized_failed_response, "{\"f\":[1,2,3,4,5],\"v\":\"1.0\"}");
  EXPECT_EQ(bytes_buffer.capacity, bytes_buffer.bytes->size());
  EXPECT_EQ(bytes_buffer.length, bytes_buffer.bytes->size());
}

TEST(FrontEndUtilsTest, TransactionExecutionPhaseToString) {
  string output;
  EXPECT_EQ(FrontEndUtils::ToString(TransactionExecutionPhase::Begin, output),
            SuccessExecutionResult());
  EXPECT_EQ(output, "BEGIN");
  EXPECT_EQ(FrontEndUtils::ToString(TransactionExecutionPhase::Prepare, output),
            SuccessExecutionResult());
  EXPECT_EQ(output, "PREPARE");
  EXPECT_EQ(FrontEndUtils::ToString(TransactionExecutionPhase::Commit, output),
            SuccessExecutionResult());
  EXPECT_EQ(output, "COMMIT");
  EXPECT_EQ(FrontEndUtils::ToString(TransactionExecutionPhase::Notify, output),
            SuccessExecutionResult());
  EXPECT_EQ(output, "NOTIFY");
  EXPECT_EQ(FrontEndUtils::ToString(TransactionExecutionPhase::Abort, output),
            SuccessExecutionResult());
  EXPECT_EQ(output, "ABORT");
  EXPECT_EQ(FrontEndUtils::ToString(TransactionExecutionPhase::End, output),
            SuccessExecutionResult());
  EXPECT_EQ(output, "END");
  EXPECT_EQ(FrontEndUtils::ToString(TransactionExecutionPhase::Unknown, output),
            SuccessExecutionResult());
  EXPECT_EQ(output, "UNKNOWN");
}

TEST(FrontEndUtilsTest, TransactionExecutionPhaseFromString) {
  TransactionExecutionPhase transaction_execution_phase;
  string input = "BEGIN";
  EXPECT_EQ(FrontEndUtils::FromString(input, transaction_execution_phase),
            SuccessExecutionResult());
  EXPECT_EQ(transaction_execution_phase, TransactionExecutionPhase::Begin);

  input = "PREPARE";
  EXPECT_EQ(FrontEndUtils::FromString(input, transaction_execution_phase),
            SuccessExecutionResult());
  EXPECT_EQ(transaction_execution_phase, TransactionExecutionPhase::Prepare);

  input = "COMMIT";
  EXPECT_EQ(FrontEndUtils::FromString(input, transaction_execution_phase),
            SuccessExecutionResult());
  EXPECT_EQ(transaction_execution_phase, TransactionExecutionPhase::Commit);

  input = "NOTIFY";
  EXPECT_EQ(FrontEndUtils::FromString(input, transaction_execution_phase),
            SuccessExecutionResult());
  EXPECT_EQ(transaction_execution_phase, TransactionExecutionPhase::Notify);

  input = "ABORT";
  EXPECT_EQ(FrontEndUtils::FromString(input, transaction_execution_phase),
            SuccessExecutionResult());
  EXPECT_EQ(transaction_execution_phase, TransactionExecutionPhase::Abort);

  input = "END";
  EXPECT_EQ(FrontEndUtils::FromString(input, transaction_execution_phase),
            SuccessExecutionResult());
  EXPECT_EQ(transaction_execution_phase, TransactionExecutionPhase::End);

  input = "UNKNOWN";
  EXPECT_EQ(FrontEndUtils::FromString(input, transaction_execution_phase),
            SuccessExecutionResult());
  EXPECT_EQ(transaction_execution_phase, TransactionExecutionPhase::Unknown);

  input = "dsadasd";
  EXPECT_EQ(FrontEndUtils::FromString(input, transaction_execution_phase),
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
  EXPECT_EQ(transaction_execution_phase, TransactionExecutionPhase::Unknown);
}

TEST(FrontEndUtilsTest, SerializePendingTransactionCount) {
  core::GetTransactionManagerStatusResponse response = {
      .pending_transactions_count = 123};

  BytesBuffer bytes_buffer;

  EXPECT_EQ(
      FrontEndUtils::SerializePendingTransactionCount(response, bytes_buffer),
      SuccessExecutionResult());

  string serialized_failed_response(bytes_buffer.bytes->begin(),
                                    bytes_buffer.bytes->end());

  EXPECT_EQ(serialized_failed_response,
            "{\"pending_transactions_count\":123,\"v\":\"1.0\"}");
  EXPECT_EQ(bytes_buffer.capacity, bytes_buffer.bytes->size());
  EXPECT_EQ(bytes_buffer.length, bytes_buffer.bytes->size());
}

}  // namespace google::scp::pbs::test
