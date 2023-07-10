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

package com.google.scp.shared.testutils.common;

import static com.google.common.truth.Truth.assertThat;

import com.google.common.base.Converter;
import com.google.inject.Inject;
import org.junit.Test;

/** Used to reduce redundancy in Converter tests */
public abstract class ConverterTest<TFrom, TTo> extends BaseModuleTest {

  public @Inject Converter<TFrom, TTo> target;

  /** Gets the object to test conversion from */
  public abstract TFrom getFrom();

  /** Gets the object to test the result of the conversion */
  public abstract TTo getTo();

  /** Tests the forward operation of the Converter */
  @Test
  public void testDoForward() {
    try {
      TTo result = target.convert(getFrom());
      assertThat(result).isEqualTo(getTo());
    } catch (UnsupportedOperationException e) {
      /** Ignore since this operation is not supported */
    }
  }

  /** Tests the backward operation of the Converter */
  @Test
  public void testDoBackward() {
    try {
      TFrom result = target.reverse().convert(getTo());
      assertThat(result).isEqualTo(getFrom());
    } catch (UnsupportedOperationException e) {
      /** Ignore since this operation is not supported */
    }
  }
}
