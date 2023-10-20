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

package com.google.scp.operator.cpio.metricclient.model;

import com.fasterxml.jackson.annotation.JsonProperty;
import com.google.auto.value.AutoValue;
import com.google.common.collect.ImmutableMap;

/** Class to represent a custom metric for reporting. */
@AutoValue
public abstract class CustomMetric {

  /** Returns a new instance of the builder for this class. */
  public static Builder builder() {
    return new AutoValue_CustomMetric.Builder();
  }

  /**
   * Metric namespace.
   *
   * <p>Used for populating namespace in {@code PutMetricDataRequest}:
   * https://sdk.amazonaws.com/java/api/latest/software/amazon/awssdk/services/cloudwatch/model/PutMetricDataRequest.html
   *
   * <p>Ignored for gcp monitoring.
   */
  @JsonProperty("name_space")
  public abstract String nameSpace();

  /** Metric name. Also used as display name in GCP monitoring. */
  @JsonProperty("name")
  public abstract String name();

  /** Metric value. */
  @JsonProperty("value")
  public abstract double value();

  /**
   * Metric value unit.
   *
   * <p>For available set of units in cloudwatch refer to:
   * https://sdk.amazonaws.com/java/api/latest/software/amazon/awssdk/services/cloudwatch/model/StandardUnit.html
   */
  @JsonProperty("unit")
  public abstract String unit();

  /**
   * A set of key-value pairs. The key represents label name and the value represents label value.
   *
   * <p>In cloudwatch this is used for populating dimensions:
   * https://sdk.amazonaws.com/java/api/latest/software/amazon/awssdk/services/cloudwatch/model/MetricDatum.html
   */
  @JsonProperty("labels")
  public abstract ImmutableMap<String, String> labels();

  /** Builder for the {@code CustomMetric} class. */
  @AutoValue.Builder
  public abstract static class Builder {

    /**
     * Set the metric namespace.
     *
     * <p>Used for populating namespace in {@code PutMetricDataRequest}:
     * https://sdk.amazonaws.com/java/api/latest/software/amazon/awssdk/services/cloudwatch/model/PutMetricDataRequest.html
     *
     * <p>Ignored for gcp monitoring.
     */
    @JsonProperty("name_space")
    public abstract Builder setNameSpace(String nameSpace);

    /** Set the metric name. Also used as display name in GCP monitoring. */
    @JsonProperty("name")
    public abstract Builder setName(String name);

    /** Set the metric value. */
    @JsonProperty("value")
    public abstract Builder setValue(double value);

    /**
     * Set the metric value unit.
     *
     * <p>For available set of units in cloudwatch refer to:
     * https://sdk.amazonaws.com/java/api/latest/software/amazon/awssdk/services/cloudwatch/model/StandardUnit.html
     */
    @JsonProperty("unit")
    public abstract Builder setUnit(String unit);

    /**
     * Set the key-value pairs representing labels. The key represents label name and the value
     * represents label value.
     *
     * <p>In cloudwatch this is used for populating dimensions:
     * https://sdk.amazonaws.com/java/api/latest/software/amazon/awssdk/services/cloudwatch/model/MetricDatum.html
     */
    @JsonProperty("labels")
    public abstract Builder setLabels(ImmutableMap<String, String> labels);

    /**
     * A builder class for creating key-value pairs representing labels. The key represents label
     * name and the value represents label value.
     *
     * <p>In cloudwatch this is used for populating dimensions:
     * https://sdk.amazonaws.com/java/api/latest/software/amazon/awssdk/services/cloudwatch/model/MetricDatum.html
     */
    abstract ImmutableMap.Builder<String, String> labelsBuilder();

    /** Adds a new label. */
    public Builder addLabel(String key, String value) {
      labelsBuilder().put(key, value);
      return this;
    }

    /** Creates a new instance of the {@code CustomMetric} class from the builder. */
    public abstract CustomMetric build();
  }
}
