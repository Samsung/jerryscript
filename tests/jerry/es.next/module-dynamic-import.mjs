/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

function check_syntax_error(code)
{
  try {
    eval(code);
    assert(false);
  } catch (e) {
    assert(e instanceof SyntaxError);
  }
}

check_syntax_error("import('a.mjs',4)");
check_syntax_error("(import('a.mjs',4))");
check_syntax_error("(import 'a.mjs')");

var successCount = 0;

import(Symbol()).then(() => assert(false), () => ++successCount)

var a = import("No such module")
a.then(() => assert(false), () => ++successCount)

/* Note: path info is stored for modules only */
import("module-export-04.mjs").then(
  (v) => {
    assert(v.x === 41)
    ++successCount
  },
  () => assert(false)
)

function f() {
  import("module-export-04.mjs").then(
    (v) => {
      assert(v.x === 41)
      ++successCount
    },
    () => assert(false)
  )
}

function g() {
  f()
}

f()

globalThis.__checkAsync = function() {
  assert(successCount === 4)
}
