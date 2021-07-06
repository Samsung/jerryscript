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

var object1 = {
  a: 1,
  b: 2,
  c: 3
};

var object2 = Object.assign ({c: 4, d: 5}, object1);

assert (JSON.stringify (object2) === '{"c":3,"d":5,"a":1,"b":2}');
assert (object2.c === 3);
assert (object2.d === 5);

// Cloning an object
var obj = { a: 1 };
var copy = Object.assign ({}, obj);
assert (JSON.stringify (copy) === '{"a":1}'); // { a: 1 }

// Warning for Deep Clone
function deepClone() {
  'use strict';

  var obj1 = { a: 0 , b: { c: 0}};
  var obj2 = Object.assign ({}, obj1);
  assert (JSON.stringify (obj2) === '{"a":0,"b":{"c":0}}');

  obj1.a = 1;
  assert (JSON.stringify (obj1) === '{"a":1,"b":{"c":0}}');
  assert (JSON.stringify (obj2) === '{"a":0,"b":{"c":0}}');

  obj2.a = 2;
  assert (JSON.stringify (obj1) === '{"a":1,"b":{"c":0}}');
  assert (JSON.stringify (obj2) === '{"a":2,"b":{"c":0}}');

  obj2.b.c = 3;
  assert (JSON.stringify (obj1) === '{"a":1,"b":{"c":3}}');
  assert (JSON.stringify (obj2) === '{"a":2,"b":{"c":3}}');

  // Deep Clone
  obj1 = { a: 0 , b: { c: 0}};
  var obj3 = JSON.parse (JSON.stringify (obj1));
  obj1.a = 4;
  obj1.b.c = 4;
  assert (JSON.stringify (obj3) === '{"a":0,"b":{"c":0}}');
}

deepClone();

// Merging objects
var o1 = { a: 1 };
var o2 = { b: 2 };
var o3 = { c: 3 };

var obj = Object.assign (o1, o2, o3);
assert (JSON.stringify (obj) === '{"a":1,"b":2,"c":3}');
assert (JSON.stringify (o1) === '{"a":1,"b":2,"c":3}');  //target object itself is changed.

//Merging objects with same properties
var o1 = { a: 1, b: 1, c: 1 };
var o2 = { b: 2, c: 2 };
var o3 = { c: 3 };

var obj = Object.assign ({}, o1, o2, o3);
assert (JSON.stringify (obj) === '{"a":1,"b":2,"c":3}');

// Properties on the prototype chain and non-enumerable properties cannot be copied
var obj = Object.create({ foo: 1 }, { // foo is on obj's prototype chain.
  bar: {
    value: 2  // bar is a non-enumerable property.
  },
  baz: {
    value: 3,
    enumerable: true  // baz is an own enumerable property.
  }
});

var copy = Object.assign ({}, obj);
assert (JSON.stringify (copy) === '{"baz":3}');

// Primitives will be wrapped to objects
var v1 = 'abc';
var v2 = true;
var v3 = 10;

var obj = Object.assign ({}, v1, null, v2, undefined, v3);
// Primitives will be wrapped, null and undefined will be ignored.
// Note, only string wrappers can have own enumerable properties.
assert (JSON.stringify (obj) === '{"0":"a","1":"b","2":"c"}');

//Exceptions will interrupt the ongoing copying task
var target = Object.defineProperty ({}, 'foo', {
  value: 1,
  writable: false
}); // target.foo is a read-only property

try {
  // TypeError: "foo" is read-only,the Exception is thrown when assigning target.foo
  Object.assign (target, { bar: 2 }, { foo2: 3, foo: 3, foo3: 3 }, { baz: 4 });
  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}

assert (target.bar === 2);  // the first source was copied successfully.
assert (target.foo2 === 3); // the first property of the second source was copied successfully.
assert (target.foo === 1);  // exception is thrown here.
assert (target.foo3 === undefined); // assign method has finished, foo3 will not be copied.
assert (target.baz === undefined);  // the third source will not be copied either.

// Copying accessors
var obj = {
  foo: 1,
  get bar() {
    return 2;
  }
};

var copy = Object.assign ({}, obj);
assert (JSON.stringify (copy) === '{"foo":1,"bar":2}');
assert (copy.bar === 2); // the value of copy.bar is obj.bar's getter's return value.

// This is an assign function that copies full descriptors
function completeAssign (target, sources) {
  sources.forEach (source => {
    var descriptors = Object.keys (source).reduce ((descriptors, key) => {
      descriptors[key] = Object.getOwnPropertyDescriptor (source, key);
      return descriptors;
    }, {});

    Object.defineProperties (target, descriptors);
  });
  return target;
}

var copy = completeAssign ({}, [obj]);
assert (JSON.stringify (copy) === '{"foo":1,"bar":2}');
assert (copy.bar === 2);

// Test when target is not coercible to object
try {
  Object.assign.call (undefined);
  assert (false);
} catch (e) {
  assert (e instanceof TypeError)
}

var asd = Symbol ("asd");
var foo = Symbol ("foo");
var bar = Symbol ("bar");
var obj = {1: 5, "a": 6, [foo]: 7, [asd]: 8, [bar]: 9};
var result = Object.assign ({}, obj);
assert (result[foo] == 7);
assert (result[asd] == 8);
assert (result[bar] == 9);

obj = {}
assert(Object.assign(obj, { a:1, b:undefined, get c() {}, set d(v) {}}) == obj);
assert(Object.getOwnPropertyDescriptor(obj, "a").enumerable);
assert(Object.getOwnPropertyDescriptor(obj, "b").enumerable);
assert(Object.getOwnPropertyDescriptor(obj, "c").enumerable);
assert(Object.getOwnPropertyDescriptor(obj, "d").enumerable);
