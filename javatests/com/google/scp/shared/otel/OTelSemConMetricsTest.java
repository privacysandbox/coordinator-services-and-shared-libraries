/*
 * Copyright 2025 Google LLC
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

package com.google.scp.shared.otel;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.shared.otel.OTelSemConMetrics.LATENCY_HISTOGRAM_BOUNDARIES;

import com.google.common.collect.ImmutableList;
import org.junit.Test;

public class OTelSemConMetricsTest {
  @Test
  public void latencyHistogramBoundaries() {
    ImmutableList<Double> expected =
        ImmutableList.of(
            0.0,
            0.0001,
            0.00012589254117941674,
            0.00015848931924611136,
            0.000199526231496888,
            0.00025118864315095806,
            0.00031622776601683805,
            0.0003981071705534974,
            0.0005011872336272725,
            0.0006309573444801935,
            0.000794328234724282,
            0.0010000000000000007,
            0.0012589254117941681,
            0.0015848931924611145,
            0.001995262314968881,
            0.002511886431509582,
            0.0031622776601683824,
            0.003981071705534976,
            0.005011872336272727,
            0.00630957344480194,
            0.007943282347242824,
            0.010000000000000012,
            0.012589254117941687,
            0.015848931924611155,
            0.019952623149688823,
            0.025118864315095836,
            0.03162277660168384,
            0.03981071705534978,
            0.050118723362727303,
            0.06309573444801943,
            0.07943282347242828,
            0.10000000000000017,
            0.12589254117941695,
            0.15848931924611165,
            0.19952623149688836,
            0.2511886431509585,
            0.31622776601683855,
            0.3981071705534981,
            0.5011872336272734,
            0.6309573444801946,
            0.7943282347242833,
            1.0000000000000022,
            1.2589254117941702,
            1.5848931924611172,
            1.9952623149688844,
            2.5118864315095863,
            3.1622776601683875,
            3.981071705534983,
            5.011872336272736,
            6.30957344480195,
            7.943282347242837,
            10.000000000000028,
            12.589254117941708,
            15.848931924611183,
            19.952623149688858,
            25.11886431509588,
            31.622776601683892,
            39.810717055349855,
            50.11872336272739,
            63.095734448019535,
            79.43282347242842,
            100.00000000000034,
            125.89254117941718,
            158.4893192461119,
            199.5262314968887,
            251.18864315095894,
            316.22776601683915,
            398.10717055349875,
            501.18723362727417,
            630.9573444801956,
            794.3282347242847,
            1000.000000000004,
            1258.9254117941723,
            1584.89319246112,
            1995.2623149688877,
            2511.8864315095907,
            3162.2776601683927,
            3981.0717055349896);

    assertThat(LATENCY_HISTOGRAM_BOUNDARIES).containsExactlyElementsIn(expected).inOrder();
  }
}
