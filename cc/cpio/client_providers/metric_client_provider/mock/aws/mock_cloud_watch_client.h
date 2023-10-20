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

#include <algorithm>
#include <memory>

#include <aws/core/NoResult.h>
#include <aws/monitoring/CloudWatchClient.h>
#include <aws/monitoring/model/PutMetricDataRequest.h>

#include "absl/algorithm/container.h"

namespace google::scp::cpio::client_providers::mock {
class MockCloudWatchClient : public Aws::CloudWatch::CloudWatchClient {
 public:
  std::function<void(
      const Aws::CloudWatch::Model::PutMetricDataRequest&,
      const Aws::CloudWatch::PutMetricDataResponseReceivedHandler&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)>
      put_metric_data_async_mock;

  void PutMetricDataAsync(
      const Aws::CloudWatch::Model::PutMetricDataRequest& request,
      const Aws::CloudWatch::PutMetricDataResponseReceivedHandler& handler,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context =
          nullptr) const override {
    if (put_metric_data_async_mock) {
      return put_metric_data_async_mock(request, handler, context);
    }

    handler(this, request, put_metric_data_outcome_mock, context);
  }

  Aws::CloudWatch::Model::PutMetricDataRequest put_metric_data_request_mock;
  Aws::CloudWatch::Model::PutMetricDataOutcome put_metric_data_outcome_mock =
      Aws::CloudWatch::Model::PutMetricDataOutcome(Aws::NoResult());

 private:
  bool IsEqual(
      const Aws::Vector<Aws::CloudWatch::Model::MetricDatum>& v1,
      const Aws::Vector<Aws::CloudWatch::Model::MetricDatum>& v2) const {
    if (v1.size() == 0 && v2.size() == 0) {
      return true;
    }
    if (v1.size() == 0 || v2.size() == 0) {
      return false;
    }

    // Current MetricClient only supports one metric_data request, so
    // here only compares the first metric_data.
    Aws::CloudWatch::Model::MetricDatum datum_v1 = v1[0];
    Aws::CloudWatch::Model::MetricDatum datum_v2 = v2[0];

    // For metric_data, only compares metric_name, value,
    // unit, and dimensions. Skips time_stamp.
    return datum_v1.GetMetricName() == datum_v2.GetMetricName() &&
           datum_v1.GetValue() == datum_v2.GetValue() &&
           datum_v1.GetUnit() == datum_v2.GetUnit() &&
           IsEqual(datum_v1.GetDimensions(), datum_v2.GetDimensions());
  }

  bool IsEqual(const Aws::Vector<Aws::CloudWatch::Model::Dimension>& v1,
               const Aws::Vector<Aws::CloudWatch::Model::Dimension>& v2) const {
    return absl::c_is_permutation(v1, v2,
                                  [](Aws::CloudWatch::Model::Dimension d1,
                                     Aws::CloudWatch::Model::Dimension d2) {
                                    return d1.GetName() == d2.GetName() &&
                                           d1.GetValue() == d2.GetValue();
                                  });
  }
};
}  // namespace google::scp::cpio::client_providers::mock
