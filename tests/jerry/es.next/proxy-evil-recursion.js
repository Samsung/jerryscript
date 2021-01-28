// Copyright JS Foundation and other contributors, http://js.foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/* Stack limit check must be enabled to run this test correctly! */

function expect_error(error_type, method) {
  try {
    method();
    assert(false);
  } catch (ex) {
    assert (ex instanceof error_type);
  }
}

/**
 * Create a proxy with recursion via prototype
 */
var proxy_one = new Proxy({length:2}, {});

Reflect.setPrototypeOf(proxy_one, proxy_one); /* [[SetPrototypeOf]] does not trigger recursion */
// aka. proxy_one.__proto__ = proxy_one; /* [[SetPrototypeOf]] */

function test_proxy_internals_using_proto(proxy_obj) {
  /* These methods can be recursive calls via prototype */
  expect_error(RangeError, function() { proxy_obj[1]; /* [[Get]] */ });
  expect_error(RangeError, function() { proxy_obj[1] = 2; /* [[Set]] */ });
  expect_error(RangeError, function() { 3 in proxy_obj; /* [[Has]] */ });
  expect_error(RangeError, function() { Reflect.has(proxy_obj, 3); /* [[Has]] */ });
}

test_proxy_internals_using_proto(proxy_one);

function test_proxy_internals_not_using_proto(proxy_obj) {
  /* The methods below should not trigger recursion via __proto__ */
  delete proxy_obj.none; /* [[Delete]] */
  delete proxy_obj["other"]; /* [[Delete]] */

  Object.getPrototypeOf(proxy_obj); /* [[GetPrototypeOf]] */
  Reflect.isExtensible(proxy_obj); /* [[IsExtensible]] */
  Object.preventExtensions(proxy_obj); /* [[PreventExtensions]] */
  Reflect.getOwnPropertyDescriptor(proxy_obj, "key"); /* [[GetOwnProperty]] */
  Reflect.defineProperty(proxy_obj, "key2", { value: 4}); /* [[DefineOwnProperty]] */
  Reflect.ownKeys(proxy_obj); /* [[OwnPropertyKeys]] */
}

test_proxy_internals_not_using_proto(proxy_one);

expect_error(TypeError, function() { proxy_one(3, 4); /* [[Call]] */ });
expect_error(TypeError, function() { new proxy_one(4); /* [[Construct]] */ });

/* Special handler to trigger proxy recursion, WARNING DO NOT use in production code! */
var handler = {
  is_handler: true,
  proxy_target: false,
  counts: {
    get: 0,
    set: 0,
    has: 0,
    deleteProperty: 0,
    getPrototypeOf: 0,
    setPrototypeOf: 0,
    isExtensible: 0,
    preventExtensions: 0,
    getOwnPropertyDescriptor: 0,
    defineProperty: 0,
    ownKeys: 0,
  },
  get: function(target, key) {
    assert(this.is_handler);
    assert(this.proxy_target !== false);
    this.counts.get++;
    return this.proxy_target.key;
  },
  set: function(target, key, value) {
    assert(this.is_handler);
    assert(this.proxy_target !== false);
    this.counts.set++;
    return this.proxy_target.key = value;
  },
  has: function(target, key) {
    assert(this.is_handler);
    assert(this.proxy_target !== false);
    this.counts.has++;
    return Reflect.has(this.proxy_target, key);
  },
  deleteProperty: function(target, key) {
    assert(this.is_handler);
    assert(this.proxy_target !== false);
    this.counts.deleteProperty++;
    return Reflect.deleteProperty(this.proxy_target, key);
  },
  getPrototypeOf: function(target) {
    assert(this.is_handler);
    assert(this.proxy_target !== false);
    this.counts.getPrototypeOf++;
    return Reflect.getPrototypeOf(this.proxy_target);
  },
  setPrototypeOf: function(target, newproto) {
    assert(this.is_handler);
    assert(this.proxy_target !== false);
    this.counts.setPrototypeOf++;
    return Reflect.setPrototypeOf(this.proxy_target, newproto);
  },
  isExtensible: function(target) {
    assert(this.is_handler);
    assert(this.proxy_target !== false);
    this.counts.isExtensible++;
    return Reflect.isExtensible(this.proxy_target);
  },
  preventExtensions: function(target) {
    assert(this.is_handler);
    assert(this.proxy_target !== false);
    this.counts.preventExtensions++;
    return Reflect.preventExtensions(this.proxy_target);
  },
  getOwnPropertyDescriptor: function(target, key) {
    assert(this.is_handler);
    assert(this.proxy_target !== false);
    this.counts.getOwnPropertyDescriptor++;
    return Reflect.getOwnPropertyDescriptor(this.proxy_target, key);
  },
  defineProperty: function(target, key, attrs) {
    assert(this.is_handler);
    assert(this.proxy_target !== false);
    this.counts.defineProperty++;
    return Reflect.defineProperty(this.proxy_target, key, attrs);
  },
  ownKeys: function(target) {
    assert(this.is_handler);
    assert(this.proxy_target !== false);
    this.counts.ownKeys++;
    return Reflect.ownKeys(this.proxy_target);
  }
};


var proxy_two = new Proxy({length:2}, handler);

handler.proxy_target = proxy_two;

test_proxy_internals_using_proto(proxy_two);

expect_error(RangeError, function() { delete proxy_two.none; /* [[Delete]] */ });
expect_error(RangeError, function() { delete proxy_two["other"]; /* [[Delete]] */ });
expect_error(RangeError, function() { Object.getPrototypeOf(proxy_two); /* [[GetPrototypeOf]] */ });
expect_error(RangeError, function() { Object.setPrototypeOf(proxy_two, {}); /* [[GetPrototypeOf]] */ });
expect_error(RangeError, function() { Reflect.isExtensible(proxy_two); /* [[IsExtensible]] */ });
expect_error(RangeError, function() { Object.preventExtensions(proxy_two); /* [[PreventExtensions]] */ });
expect_error(RangeError, function() { Reflect.getOwnPropertyDescriptor(proxy_two, "key"); /* [[GetOwnProperty]] */ });
expect_error(RangeError, function() { Reflect.defineProperty(proxy_two, "key2", { value: 4}); /* [[DefineOwnProperty]] */ });
expect_error(RangeError, function() { Reflect.ownKeys(proxy_two); /* [[OwnPropertyKeys]] */ });
