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

var builtin_objects = [
  Array,
  ArrayBuffer,
  Boolean,
  DataView,
  Date,
  Error,
  EvalError,
  Function,
  Map,
  Number,
  Object,
  Promise,
  RangeError,
  ReferenceError,
  RegExp,
  Set,
  SharedArrayBuffer,
  String,
  Symbol,
  SyntaxError,
  TypeError,
  URIError,
  WeakMap,
  WeakSet,
];

var builtin_typedArrays = [
  Float32Array,
  Float64Array,
  Int16Array,
  Int32Array,
  Int8Array,
  Uint16Array,
  Uint32Array,
  Uint8Array,
  Uint8ClampedArray,
];


(function () {
  /* each length property is configurable */
  var desc;
  
  for (obj of builtin_objects) {
    desc = Object.getOwnPropertyDescriptor(obj, 'length');                                                                                                                    
    assert(desc.writable === false);
    assert(desc.enumerable === false);
    assert(desc.configurable === true);
  }
  
  for (ta of builtin_typedArrays) {
    desc = Object.getOwnPropertyDescriptor(ta, 'length');                                                                                                                    
    assert(desc.writable === false);
    assert(desc.enumerable === false);
    assert(desc.configurable === true);
  }
})();

(function () {
  /* each length property can be deleted */
  for (obj of builtin_objects) {
    assert(obj.hasOwnProperty('length') === true);
    assert(delete obj.length);
    assert(obj.hasOwnProperty('length') === false);
  }

  for (ta of builtin_typedArrays) {
    assert(ta.hasOwnProperty('length') === true);
    assert(delete ta.length);
    assert(ta.hasOwnProperty('length') === false);
  }
})();

(function () {
  /* test length property of builtin function */
  for (obj of builtin_objects) {
    var property_names = Object.getOwnPropertyNames(obj);
    for (var name of property_names) {
      if (typeof obj[name] == 'function') {
        var func = obj[name];
        var desc = Object.getOwnPropertyDescriptor(func, 'length');
        assert(desc.writable === false);
        assert(desc.enumerable === false);
        assert(desc.configurable === true);

        assert(func.hasOwnProperty('length') === true);
        assert(delete func.length);
        assert(func.hasOwnProperty('length') === false);
      }
    }
  }
})();

(function () {
  /* test length property of function objects */
  var normal_func = function () {};
  var arrow_func = () => {};
  var bound_func = normal_func.bind({});
  var nested_bound_func = arrow_func.bind().bind(1);

  var functions = [normal_func, arrow_func, bound_func, nested_bound_func];

  for (func of functions) {
    var desc = Object.getOwnPropertyDescriptor(func, 'length');
    assert(desc.writable === false);
    assert(desc.enumerable === false);
    assert(desc.configurable === true);
  }

  for (func of functions) {
    assert(func.hasOwnProperty('length') === true);
    assert(delete func.length);
    assert(func.hasOwnProperty('length') === false);
  }
})();

(function() {
  /* changing the length of f affects the bound function */
  function f(a,b,c) {}
  Object.defineProperty(f, "length", { value: 30 });
  var g = f.bind(1,2)
  assert(g.length === 29);
})();

(function() {
  /* changing the length of f does not affect the bound function */
  function f(a,b,c) {}
  var g = f.bind(1,2)
  Object.defineProperty(f, "length", { value: 30 });
  assert(g.length === 2);
})();
