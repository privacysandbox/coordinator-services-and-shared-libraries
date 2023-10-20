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
#ifndef SCP_CPIO_TEST_CPIO_OPTIONS_H_
#define SCP_CPIO_TEST_CPIO_OPTIONS_H_

#include <string>

#include "public/cpio/interface/type_def.h"

namespace google::scp::cpio {
/// Global CPIO options to test CPIO.
struct TestCpioOptions : public CpioOptions {
  /// Cloud owner ID.
  std::string owner_id;
  /// Cloud zone.
  std::string zone;
  /// Cloud region.
  std::string region;
  /// Instance ID.
  std::string instance_id;
  /// Public IP address.
  std::string public_ipv4_address;
  /// Private IP address.
  std::string private_ipv4_address;
  /// STS client endpoint override.
  std::string sts_endpoint_override;

  CpioOptions ToCpioOptions() {
    CpioOptions cpio_options;
    cpio_options.log_option = CpioOptions::log_option;
    return cpio_options;
  }
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_TEST_CPIO_OPTIONS_H_
