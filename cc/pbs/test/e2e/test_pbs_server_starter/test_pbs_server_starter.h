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

#pragma once

#include <map>
#include <string>

namespace google::scp::pbs::test::e2e {
enum struct AuthResult {
  kSuccess = 1,
  kForbidden = 2,
};

struct TestPbsDataConfig {
  std::string region;
  std::string network_name;

  std::string localstack_container_name;
  std::string localstack_port;

  std::string reporting_origin;
};

struct TestPbsConfig {
  std::string pbs1_container_name;
  std::string pbs1_port;
  std::string pbs1_health_port;
  std::string pbs1_budget_key_table_name;
  std::string pbs1_partition_lock_table_name;
  std::string pbs1_journal_bucket_name;

  std::string pbs2_container_name;
  std::string pbs2_port;
  std::string pbs2_health_port;
  std::string pbs2_budget_key_table_name;
  std::string pbs2_partition_lock_table_name;
  std::string pbs2_journal_bucket_name;
};

class TestPbsServerStarter {
 public:
  explicit TestPbsServerStarter(const TestPbsDataConfig& config)
      : config_(config) {}

  int Setup();

  int RunTwoPbsServers(const TestPbsConfig& config, bool setup_data,
                       std::map<std::string, std::string> env_overrides = {});
  int RunPbsServer1(const TestPbsConfig& config,
                    std::map<std::string, std::string> env_overrides = {});
  int RunPbsServer2(const TestPbsConfig& config,
                    std::map<std::string, std::string> env_overrides = {});

  void StopTwoPbsServers(const TestPbsConfig& config);
  void StopPbsServer1(const TestPbsConfig& config);
  void StopPbsServer2(const TestPbsConfig& config);

  void Teardown();

 private:
  std::map<std::string, std::string> CreatePbsEnvVariables(
      const std::string& budget_key_table,
      const std::string& partition_lock_table,
      const std::string& journal_bucket, const std::string& host_port,
      const std::string& health_port, const std::string& remote_host,
      const std::string& remote_host_port);

  TestPbsDataConfig config_;
};
}  // namespace google::scp::pbs::test::e2e
