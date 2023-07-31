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

import com.google.acai.Acai;
import com.google.inject.Module;
import org.junit.Rule;

/** Can be extended by tests that require Acai modules */
public abstract class BaseModuleTest {

  @Rule public Acai acai = new Acai(getModule());

  /** Gets the module to inject in the test */
  public abstract Class<? extends Module> getModule();
}
