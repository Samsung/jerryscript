# Copyright JS Foundation and other contributors, http://js.foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required (VERSION 2.8.12)
set(JERRY_CORE_NAME jerry-core)
project (${JERRY_CORE_NAME} C)

include(CheckLibraryExists)

# Optional features
set(JERRY_CPOINTER_32_BIT           OFF          CACHE BOOL   "Enable 32 bit compressed pointers?")
set(JERRY_DEBUGGER                  OFF          CACHE BOOL   "Enable JerryScript debugger?")
set(JERRY_ERROR_MESSAGES            OFF          CACHE BOOL   "Enable error messages?")
set(JERRY_EXTERNAL_CONTEXT          OFF          CACHE BOOL   "Enable external context?")
set(JERRY_PARSER                    ON           CACHE BOOL   "Enable javascript-parser?")
set(JERRY_LINE_INFO                 OFF          CACHE BOOL   "Enable line info?")
set(JERRY_LOGGING                   OFF          CACHE BOOL   "Enable logging?")
set(JERRY_MEM_STATS                 OFF          CACHE BOOL   "Enable memory statistics?")
set(JERRY_MEM_GC_BEFORE_EACH_ALLOC  OFF          CACHE BOOL   "Enable mem-stress test?")
set(JERRY_PARSER_DUMP_BYTE_CODE     OFF          CACHE BOOL   "Enable parser byte-code dumps?")
set(JERRY_PROFILE                   "es.next"    CACHE STRING "Use default or other profile?")
set(JERRY_PROMISE_CALLBACK          OFF          CACHE BOOL   "Enable Promise callbacks?")
set(JERRY_REGEXP_STRICT_MODE        OFF          CACHE BOOL   "Enable regexp strict mode?")
set(JERRY_REGEXP_DUMP_BYTE_CODE     OFF          CACHE BOOL   "Enable regexp byte-code dumps?")
set(JERRY_SNAPSHOT_EXEC             OFF          CACHE BOOL   "Enable executing snapshot files?")
set(JERRY_SNAPSHOT_SAVE             OFF          CACHE BOOL   "Enable saving snapshot files?")
set(JERRY_SYSTEM_ALLOCATOR          OFF          CACHE BOOL   "Enable system allocator?")
set(JERRY_VALGRIND                  OFF          CACHE BOOL   "Enable Valgrind support?")
set(JERRY_VM_EXEC_STOP              OFF          CACHE BOOL   "Enable VM execution stopping?")
set(JERRY_GLOBAL_HEAP_SIZE          "(512)"      CACHE STRING "Size of memory heap, in kilobytes")
set(JERRY_GC_LIMIT                  "(0)"        CACHE STRING "Heap usage limit to trigger garbage collection")
set(JERRY_STACK_LIMIT               "(0)"        CACHE STRING "Maximum stack usage size, in kilobytes")
set(JERRY_GC_MARK_LIMIT             "(8)"        CACHE STRING "Maximum depth of recursion during GC mark phase")

# Option overrides
if(JERRY_SYSTEM_ALLOCATOR)
  set(JERRY_CPOINTER_32_BIT ON)

  set(JERRY_CPOINTER_32_BIT_MESSAGE " (FORCED BY SYSTEM ALLOCATOR)")
endif()

if (JERRY_GLOBAL_HEAP_SIZE GREATER 512)
  set(JERRY_CPOINTER_32_BIT ON)

  set(JERRY_CPOINTER_32_BIT_MESSAGE " (FORCED BY HEAP SIZE)")
endif()

if(NOT JERRY_PARSER)
  set(JERRY_SNAPSHOT_EXEC ON)
  set(JERRY_PARSER_DUMP   OFF)

  set(JERRY_SNAPSHOT_EXEC_MESSAGE " (FORCED BY DISABLED JS PARSER)")
  set(JERRY_PARSER_DUMP_MESSAGE   " (FORCED BY DISABLED JS PARSER)")
endif()

if(JERRY_CMDLINE_SNAPSHOT)
  set(JERRY_SNAPSHOT_SAVE ON)

  set(JERRY_SNAPSHOT_SAVE_MESSAGE " (FORCED BY SNAPSHOT TOOL)")
endif()

if(JERRY_MEM_STATS OR JERRY_PARSER_DUMP_BYTE_CODE OR JERRY_REGEXP_DUMP_BYTE_CODE)
  set(JERRY_LOGGING ON)

  set(JERRYRE_LOGGING_MESSAGE " (FORCED BY STATS OR DUMP)")
endif()

# Status messages
message(STATUS "JERRY_CPOINTER_32_BIT          " ${JERRY_CPOINTER_32_BIT} ${JERRY_CPOINTER_32_BIT_MESSAGE})
message(STATUS "JERRY_DEBUGGER                 " ${JERRY_DEBUGGER})
message(STATUS "JERRY_ERROR_MESSAGES           " ${JERRY_ERROR_MESSAGES})
message(STATUS "JERRY_EXTERNAL_CONTEXT         " ${JERRY_EXTERNAL_CONTEXT})
message(STATUS "JERRY_PARSER                   " ${JERRY_PARSER})
message(STATUS "JERRY_LINE_INFO                " ${JERRY_LINE_INFO})
message(STATUS "JERRY_LOGGING                  " ${JERRY_LOGGING} ${JERRY_LOGGING_MESSAGE})
message(STATUS "JERRY_MEM_STATS                " ${JERRY_MEM_STATS})
message(STATUS "JERRY_MEM_GC_BEFORE_EACH_ALLOC " ${JERRY_MEM_GC_BEFORE_EACH_ALLOC})
message(STATUS "JERRY_PARSER_DUMP_BYTE_CODE    " ${JERRY_PARSER_DUMP_BYTE_CODE} ${JERRY_PARSER_DUMP_MESSAGE})
message(STATUS "JERRY_PROFILE                  " ${JERRY_PROFILE})
message(STATUS "JERRY_PROMISE_CALLBACK         " ${JERRY_PROMISE_CALLBACK})
message(STATUS "JERRY_REGEXP_STRICT_MODE       " ${JERRY_REGEXP_STRICT_MODE})
message(STATUS "JERRY_REGEXP_DUMP_BYTE_CODE    " ${JERRY_REGEXP_DUMP_BYTE_CODE})
message(STATUS "JERRY_SNAPSHOT_EXEC            " ${JERRY_SNAPSHOT_EXEC} ${JERRY_SNAPSHOT_EXEC_MESSAGE})
message(STATUS "JERRY_SNAPSHOT_SAVE            " ${JERRY_SNAPSHOT_SAVE} ${JERRY_SNAPSHOT_SAVE_MESSAGE})
message(STATUS "JERRY_SYSTEM_ALLOCATOR         " ${JERRY_SYSTEM_ALLOCATOR})
message(STATUS "JERRY_VALGRIND                 " ${JERRY_VALGRIND})
message(STATUS "JERRY_VM_EXEC_STOP             " ${JERRY_VM_EXEC_STOP})
message(STATUS "JERRY_GLOBAL_HEAP_SIZE         " ${JERRY_GLOBAL_HEAP_SIZE})
message(STATUS "JERRY_GC_LIMIT                 " ${JERRY_GC_LIMIT})
message(STATUS "JERRY_STACK_LIMIT              " ${JERRY_STACK_LIMIT})
message(STATUS "JERRY_GC_MARK_LIMIT            " ${JERRY_GC_MARK_LIMIT})

# Include directories
set(INCLUDE_CORE_PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(INCLUDE_CORE_PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/api"
    "${CMAKE_CURRENT_SOURCE_DIR}/debugger"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecma/base"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecma/builtin-objects"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecma/builtin-objects/typedarray"
    "${CMAKE_CURRENT_SOURCE_DIR}/ecma/operations"
    "${CMAKE_CURRENT_SOURCE_DIR}/jcontext"
    "${CMAKE_CURRENT_SOURCE_DIR}/jmem"
    "${CMAKE_CURRENT_SOURCE_DIR}/jrt"
    "${CMAKE_CURRENT_SOURCE_DIR}/lit"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/js"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/regexp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vm")

set(INCLUDE_CORE_PUBLIC ${INCLUDE_CORE_PUBLIC} PARENT_SCOPE) # for jerry-port
set(INCLUDE_CORE_PRIVATE ${INCLUDE_CORE_PRIVATE} PARENT_SCOPE) # for tests/unit-core

# Sources
# Jerry core
set(SOURCE_CORE_FILES
  api/jerry-debugger-transport.c
  api/jerry-debugger.c
  api/jerry-snapshot.c
  api/jerry.c
  debugger/debugger.c
  ecma/base/ecma-alloc.c
  ecma/base/ecma-gc.c
  ecma/base/ecma-errors.c
  ecma/base/ecma-helpers-collection.c
  ecma/base/ecma-helpers-conversion.c
  ecma/base/ecma-helpers-errol.c
  ecma/base/ecma-helpers-external-pointers.c
  ecma/base/ecma-helpers-number.c
  ecma/base/ecma-helpers-string.c
  ecma/base/ecma-helpers-value.c
  ecma/base/ecma-helpers.c
  ecma/base/ecma-init-finalize.c
  ecma/base/ecma-lcache.c
  ecma/base/ecma-literal-storage.c
  ecma/base/ecma-module.c
  ecma/base/ecma-property-hashmap.c
  ecma/builtin-objects/ecma-builtin-aggregateerror.c
  ecma/builtin-objects/ecma-builtin-aggregateerror-prototype.c
  ecma/builtin-objects/ecma-builtin-array-iterator-prototype.c
  ecma/builtin-objects/ecma-builtin-array-prototype-unscopables.c
  ecma/builtin-objects/ecma-builtin-array-prototype.c
  ecma/builtin-objects/ecma-builtin-array.c
  ecma/builtin-objects/ecma-builtin-arraybuffer-prototype.c
  ecma/builtin-objects/ecma-builtin-arraybuffer.c
  ecma/builtin-objects/ecma-builtin-async-function-prototype.c
  ecma/builtin-objects/ecma-builtin-async-function.c
  ecma/builtin-objects/ecma-builtin-async-generator-function.c
  ecma/builtin-objects/ecma-builtin-async-generator-prototype.c
  ecma/builtin-objects/ecma-builtin-async-generator.c
  ecma/builtin-objects/ecma-builtin-async-iterator-prototype.c
  ecma/builtin-objects/ecma-builtin-bigint-prototype.c
  ecma/builtin-objects/ecma-builtin-bigint.c
  ecma/builtin-objects/ecma-builtin-boolean-prototype.c
  ecma/builtin-objects/ecma-builtin-boolean.c
  ecma/builtin-objects/ecma-builtin-dataview-prototype.c
  ecma/builtin-objects/ecma-builtin-dataview.c
  ecma/builtin-objects/ecma-builtin-date-prototype.c
  ecma/builtin-objects/ecma-builtin-date.c
  ecma/builtin-objects/ecma-builtin-error-prototype.c
  ecma/builtin-objects/ecma-builtin-error.c
  ecma/builtin-objects/ecma-builtin-evalerror-prototype.c
  ecma/builtin-objects/ecma-builtin-evalerror.c
  ecma/builtin-objects/ecma-builtin-function-prototype.c
  ecma/builtin-objects/ecma-builtin-function.c
  ecma/builtin-objects/ecma-builtin-generator-function.c
  ecma/builtin-objects/ecma-builtin-generator-prototype.c
  ecma/builtin-objects/ecma-builtin-generator.c
  ecma/builtin-objects/ecma-builtin-global.c
  ecma/builtin-objects/ecma-builtin-handlers.c
  ecma/builtin-objects/ecma-builtin-helpers-date.c
  ecma/builtin-objects/ecma-builtin-helpers-error.c
  ecma/builtin-objects/ecma-builtin-helpers-json.c
  ecma/builtin-objects/ecma-builtin-helpers-sort.c
  ecma/builtin-objects/ecma-builtin-helpers.c
  ecma/builtin-objects/ecma-builtin-intrinsic.c
  ecma/builtin-objects/ecma-builtin-iterator-prototype.c
  ecma/builtin-objects/ecma-builtin-json.c
  ecma/builtin-objects/ecma-builtin-map-iterator-prototype.c
  ecma/builtin-objects/ecma-builtin-map-prototype.c
  ecma/builtin-objects/ecma-builtin-map.c
  ecma/builtin-objects/ecma-builtin-math.c
  ecma/builtin-objects/ecma-builtin-number-prototype.c
  ecma/builtin-objects/ecma-builtin-number.c
  ecma/builtin-objects/ecma-builtin-object-prototype.c
  ecma/builtin-objects/ecma-builtin-object.c
  ecma/builtin-objects/ecma-builtin-promise-prototype.c
  ecma/builtin-objects/ecma-builtin-promise.c
  ecma/builtin-objects/ecma-builtin-proxy.c
  ecma/builtin-objects/ecma-builtin-rangeerror-prototype.c
  ecma/builtin-objects/ecma-builtin-rangeerror.c
  ecma/builtin-objects/ecma-builtin-referenceerror-prototype.c
  ecma/builtin-objects/ecma-builtin-referenceerror.c
  ecma/builtin-objects/ecma-builtin-reflect.c
  ecma/builtin-objects/ecma-builtin-regexp-prototype.c
  ecma/builtin-objects/ecma-builtin-regexp-string-iterator-prototype.c
  ecma/builtin-objects/ecma-builtin-regexp.c
  ecma/builtin-objects/ecma-builtin-set-iterator-prototype.c
  ecma/builtin-objects/ecma-builtin-set-prototype.c
  ecma/builtin-objects/ecma-builtin-set.c
  ecma/builtin-objects/ecma-builtin-shared-arraybuffer-prototype.c
  ecma/builtin-objects/ecma-builtin-shared-arraybuffer.c
  ecma/builtin-objects/ecma-builtin-string-iterator-prototype.c
  ecma/builtin-objects/ecma-builtin-string-prototype.c
  ecma/builtin-objects/ecma-builtin-string.c
  ecma/builtin-objects/ecma-builtin-symbol-prototype.c
  ecma/builtin-objects/ecma-builtin-symbol.c
  ecma/builtin-objects/ecma-builtin-syntaxerror-prototype.c
  ecma/builtin-objects/ecma-builtin-syntaxerror.c
  ecma/builtin-objects/ecma-builtin-type-error-thrower.c
  ecma/builtin-objects/ecma-builtin-typeerror-prototype.c
  ecma/builtin-objects/ecma-builtin-typeerror.c
  ecma/builtin-objects/ecma-builtin-urierror-prototype.c
  ecma/builtin-objects/ecma-builtin-urierror.c
  ecma/builtin-objects/ecma-builtin-weakmap-prototype.c
  ecma/builtin-objects/ecma-builtin-weakmap.c
  ecma/builtin-objects/ecma-builtin-weakref-prototype.c
  ecma/builtin-objects/ecma-builtin-weakref.c
  ecma/builtin-objects/ecma-builtin-weakset-prototype.c
  ecma/builtin-objects/ecma-builtin-weakset.c
  ecma/builtin-objects/ecma-builtins.c
  ecma/builtin-objects/typedarray/ecma-builtin-bigint64array-prototype.c
  ecma/builtin-objects/typedarray/ecma-builtin-bigint64array.c
  ecma/builtin-objects/typedarray/ecma-builtin-biguint64array-prototype.c
  ecma/builtin-objects/typedarray/ecma-builtin-biguint64array.c
  ecma/builtin-objects/typedarray/ecma-builtin-float32array-prototype.c
  ecma/builtin-objects/typedarray/ecma-builtin-float32array.c
  ecma/builtin-objects/typedarray/ecma-builtin-float64array-prototype.c
  ecma/builtin-objects/typedarray/ecma-builtin-float64array.c
  ecma/builtin-objects/typedarray/ecma-builtin-int16array-prototype.c
  ecma/builtin-objects/typedarray/ecma-builtin-int16array.c
  ecma/builtin-objects/typedarray/ecma-builtin-int32array-prototype.c
  ecma/builtin-objects/typedarray/ecma-builtin-int32array.c
  ecma/builtin-objects/typedarray/ecma-builtin-int8array-prototype.c
  ecma/builtin-objects/typedarray/ecma-builtin-int8array.c
  ecma/builtin-objects/typedarray/ecma-builtin-typedarray-helpers.c
  ecma/builtin-objects/typedarray/ecma-builtin-typedarray-prototype.c
  ecma/builtin-objects/typedarray/ecma-builtin-typedarray.c
  ecma/builtin-objects/typedarray/ecma-builtin-uint16array-prototype.c
  ecma/builtin-objects/typedarray/ecma-builtin-uint16array.c
  ecma/builtin-objects/typedarray/ecma-builtin-uint32array-prototype.c
  ecma/builtin-objects/typedarray/ecma-builtin-uint32array.c
  ecma/builtin-objects/typedarray/ecma-builtin-uint8array-prototype.c
  ecma/builtin-objects/typedarray/ecma-builtin-uint8array.c
  ecma/builtin-objects/typedarray/ecma-builtin-uint8clampedarray-prototype.c
  ecma/builtin-objects/typedarray/ecma-builtin-uint8clampedarray.c
  ecma/operations/ecma-arguments-object.c
  ecma/operations/ecma-array-object.c
  ecma/operations/ecma-arraybuffer-object.c
  ecma/operations/ecma-async-generator-object.c
  ecma/operations/ecma-big-uint.c
  ecma/operations/ecma-bigint-object.c
  ecma/operations/ecma-bigint.c
  ecma/operations/ecma-boolean-object.c
  ecma/operations/ecma-comparison.c
  ecma/operations/ecma-container-object.c
  ecma/operations/ecma-conversion.c
  ecma/operations/ecma-dataview-object.c
  ecma/operations/ecma-eval.c
  ecma/operations/ecma-exceptions.c
  ecma/operations/ecma-function-object.c
  ecma/operations/ecma-get-put-value.c
  ecma/operations/ecma-iterator-object.c
  ecma/operations/ecma-jobqueue.c
  ecma/operations/ecma-lex-env.c
  ecma/operations/ecma-number-arithmetic.c
  ecma/operations/ecma-number-object.c
  ecma/operations/ecma-objects-general.c
  ecma/operations/ecma-objects.c
  ecma/operations/ecma-promise-object.c
  ecma/operations/ecma-proxy-object.c
  ecma/operations/ecma-reference.c
  ecma/operations/ecma-regexp-object.c
  ecma/operations/ecma-shared-arraybuffer-object.c
  ecma/operations/ecma-string-object.c
  ecma/operations/ecma-symbol-object.c
  ecma/operations/ecma-typedarray-object.c
  jcontext/jcontext.c
  jmem/jmem-allocator.c
  jmem/jmem-heap.c
  jmem/jmem-poolman.c
  jrt/jrt-fatals.c
  lit/lit-char-helpers.c
  lit/lit-magic-strings.c
  lit/lit-strings.c
  parser/js/byte-code.c
  parser/js/common.c
  parser/js/js-lexer.c
  parser/js/js-parser-expr.c
  parser/js/js-parser-mem.c
  parser/js/js-parser-module.c
  parser/js/js-parser-statm.c
  parser/js/js-parser-tagged-template-literal.c
  parser/js/js-parser-util.c
  parser/js/js-parser.c
  parser/js/js-scanner-ops.c
  parser/js/js-scanner-util.c
  parser/js/js-scanner.c
  parser/regexp/re-bytecode.c
  parser/regexp/re-compiler.c
  parser/regexp/re-parser.c
  vm/opcodes-ecma-arithmetics.c
  vm/opcodes-ecma-bitwise.c
  vm/opcodes-ecma-relational-equality.c
  vm/opcodes.c
  vm/vm-stack.c
  vm/vm-utils.c
  vm/vm.c
)

# Amalgamated JerryScript source/header build.
#  The process will create the following files:
#   * jerryscript.c
#   * jerryscript.h
#   * jerryscript-config.h
if(ENABLE_AMALGAM)

  # Create single C/H file
  set(HEADER_CORE_FILES
    api/jerry-snapshot.h
    debugger/debugger.h
    ecma/base/ecma-alloc.h
    ecma/base/ecma-errors.h
    ecma/base/ecma-gc.h
    ecma/base/ecma-globals.h
    ecma/base/ecma-helpers.h
    ecma/base/ecma-init-finalize.h
    ecma/base/ecma-lcache.h
    ecma/base/ecma-literal-storage.h
    ecma/base/ecma-module.h
    ecma/base/ecma-property-hashmap.h
    ecma/builtin-objects/ecma-builtin-aggregateerror-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-aggregateerror.inc.h
    ecma/builtin-objects/ecma-builtin-array-iterator-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-array-prototype-unscopables.inc.h
    ecma/builtin-objects/ecma-builtin-array-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-array.inc.h
    ecma/builtin-objects/ecma-builtin-arraybuffer-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-arraybuffer.inc.h
    ecma/builtin-objects/ecma-builtin-async-function-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-async-function.inc.h
    ecma/builtin-objects/ecma-builtin-async-generator-function.inc.h
    ecma/builtin-objects/ecma-builtin-async-generator-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-async-generator.inc.h
    ecma/builtin-objects/ecma-builtin-async-iterator-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-bigint-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-bigint.inc.h
    ecma/builtin-objects/ecma-builtin-boolean-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-boolean.inc.h
    ecma/builtin-objects/ecma-builtin-dataview-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-dataview.inc.h
    ecma/builtin-objects/ecma-builtin-date-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-date.inc.h
    ecma/builtin-objects/ecma-builtin-error-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-error.inc.h
    ecma/builtin-objects/ecma-builtin-evalerror-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-evalerror.inc.h
    ecma/builtin-objects/ecma-builtin-function-prototype.h
    ecma/builtin-objects/ecma-builtin-function-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-function.inc.h
    ecma/builtin-objects/ecma-builtin-generator-function.inc.h
    ecma/builtin-objects/ecma-builtin-generator-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-generator.inc.h
    ecma/builtin-objects/ecma-builtin-global.inc.h
    ecma/builtin-objects/ecma-builtin-handlers.h
    ecma/builtin-objects/ecma-builtin-handlers.inc.h
    ecma/builtin-objects/ecma-builtin-helpers-macro-defines.inc.h
    ecma/builtin-objects/ecma-builtin-helpers-macro-undefs.inc.h
    ecma/builtin-objects/ecma-builtin-helpers.h
    ecma/builtin-objects/ecma-builtin-internal-routines-template.inc.h
    ecma/builtin-objects/ecma-builtin-intrinsic.inc.h
    ecma/builtin-objects/ecma-builtin-iterator-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-json.inc.h
    ecma/builtin-objects/ecma-builtin-map-iterator-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-map-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-map.inc.h
    ecma/builtin-objects/ecma-builtin-math.inc.h
    ecma/builtin-objects/ecma-builtin-number-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-number.inc.h
    ecma/builtin-objects/ecma-builtin-object-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-object.h
    ecma/builtin-objects/ecma-builtin-object.inc.h
    ecma/builtin-objects/ecma-builtin-promise-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-promise.inc.h
    ecma/builtin-objects/ecma-builtin-proxy.inc.h
    ecma/builtin-objects/ecma-builtin-rangeerror-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-rangeerror.inc.h
    ecma/builtin-objects/ecma-builtin-referenceerror-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-referenceerror.inc.h
    ecma/builtin-objects/ecma-builtin-reflect.inc.h
    ecma/builtin-objects/ecma-builtin-regexp-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-regexp-string-iterator-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-regexp.inc.h
    ecma/builtin-objects/ecma-builtin-set-iterator-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-set-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-set.inc.h
    ecma/builtin-objects/ecma-builtin-shared-arraybuffer-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-shared-arraybuffer.inc.h
    ecma/builtin-objects/ecma-builtin-string-iterator-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-string-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-string.inc.h
    ecma/builtin-objects/ecma-builtin-symbol-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-symbol.inc.h
    ecma/builtin-objects/ecma-builtin-syntaxerror-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-syntaxerror.inc.h
    ecma/builtin-objects/ecma-builtin-type-error-thrower.inc.h
    ecma/builtin-objects/ecma-builtin-typeerror-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-typeerror.inc.h
    ecma/builtin-objects/ecma-builtin-urierror-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-urierror.inc.h
    ecma/builtin-objects/ecma-builtin-weakmap-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-weakmap.inc.h
    ecma/builtin-objects/ecma-builtin-weakref-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-weakref.inc.h
    ecma/builtin-objects/ecma-builtin-weakset-prototype.inc.h
    ecma/builtin-objects/ecma-builtin-weakset.inc.h
    ecma/builtin-objects/ecma-builtins-internal.h
    ecma/builtin-objects/ecma-builtins.h
    ecma/builtin-objects/ecma-builtins.inc.h
    ecma/operations/ecma-arguments-object.h
    ecma/operations/ecma-array-object.h
    ecma/operations/ecma-arraybuffer-object.h
    ecma/operations/ecma-async-generator-object.h
    ecma/operations/ecma-big-uint.h
    ecma/operations/ecma-bigint-object.h
    ecma/operations/ecma-bigint.h
    ecma/operations/ecma-boolean-object.h
    ecma/operations/ecma-comparison.h
    ecma/operations/ecma-container-object.h
    ecma/operations/ecma-conversion.h
    ecma/operations/ecma-dataview-object.h
    ecma/operations/ecma-eval.h
    ecma/operations/ecma-exceptions.h
    ecma/operations/ecma-function-object.h
    ecma/operations/ecma-iterator-object.h
    ecma/operations/ecma-jobqueue.h
    ecma/operations/ecma-lex-env.h
    ecma/operations/ecma-number-arithmetic.h
    ecma/operations/ecma-number-object.h
    ecma/operations/ecma-objects-general.h
    ecma/operations/ecma-objects.h
    ecma/operations/ecma-promise-object.h
    ecma/operations/ecma-proxy-object.h
    ecma/operations/ecma-reference.h
    ecma/operations/ecma-regexp-object.h
    ecma/operations/ecma-shared-arraybuffer-object.h
    ecma/operations/ecma-string-object.h
    ecma/operations/ecma-symbol-object.h
    ecma/operations/ecma-typedarray-object.h
    include/jerryscript-compiler.h
    include/jerryscript-core.h
    include/jerryscript-debugger-transport.h
    include/jerryscript-debugger.h
    include/jerryscript-port.h
    include/jerryscript-snapshot.h
    include/jerryscript.h
    jcontext/jcontext.h
    jmem/jmem-allocator-internal.h
    jmem/jmem.h
    jrt/jrt-bit-fields.h
    jrt/jrt-libc-includes.h
    jrt/jrt-types.h
    jrt/jrt.h
    lit/lit-char-helpers.h
    lit/lit-globals.h
    lit/lit-magic-strings.h
    lit/lit-magic-strings.inc.h
    lit/lit-strings.h
    lit/lit-unicode-conversions-sup.inc.h
    lit/lit-unicode-conversions.inc.h
    lit/lit-unicode-folding.inc.h
    lit/lit-unicode-ranges-sup.inc.h
    lit/lit-unicode-ranges.inc.h
    vm/opcodes.h
    vm/vm-defines.h
    vm/vm-stack.h
    vm/vm.h
  )

  # Generated files
  set(AMALGAM_CORE_C "${CMAKE_BINARY_DIR}/amalgam/jerryscript.c")
  set(AMALGAM_CORE_H "${CMAKE_BINARY_DIR}/amalgam/jerryscript.h")
  set(AMALGAM_CONFIG_H "${CMAKE_BINARY_DIR}/amalgam/jerryscript-config.h")

  add_custom_command(OUTPUT ${AMALGAM_CORE_C} ${AMALGAM_CORE_H}
                     COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/tools/amalgam.py
                             --jerry-core
                             --output-dir ${CMAKE_BINARY_DIR}/amalgam
                     DEPENDS ${SOURCE_CORE_FILES}
                             ${HEADER_CORE_FILES}
                             ${CMAKE_SOURCE_DIR}/tools/amalgam.py
  )

  # The "true" jerryscript-config.h will be generated by the configure_file below,
  # which contains the default options and the ones passed for the CMake.
  # The input for this is the jerryscript-config.h generated by the command above.
  set(JERRYSCRIPT_GEN_CONFIG_H ${CMAKE_CURRENT_BINARY_DIR}/jerryscript-config.h)
  add_custom_command(OUTPUT ${AMALGAM_CONFIG_H}
                     COMMAND ${CMAKE_COMMAND} -E copy ${JERRYSCRIPT_GEN_CONFIG_H} ${AMALGAM_CONFIG_H}
                     DEPENDS ${AMALGAM_CORE_C} ${AMALGAM_CORE_H})
  add_custom_target(amalgam-jerry DEPENDS ${AMALGAM_CORE_C} ${AMALGAM_CORE_H} ${AMALGAM_CONFIG_H})
  add_dependencies(amalgam amalgam-jerry)

  set(SOURCE_CORE_FILES ${AMALGAM_CORE_C} ${AMALGAM_CORE_H} ${AMALGAM_CONFIG_H})
endif()

# Third-party
# Valgrind
set(INCLUDE_THIRD_PARTY_VALGRIND "${CMAKE_SOURCE_DIR}/third-party/valgrind")

# build mode specific compile/link flags
set(DEFINES_JERRY ${DEFINES_JERRY} $<$<NOT:$<CONFIG:Debug>>:JERRY_NDEBUG>)

# Jerry heap-section
if(DEFINED JERRY_ATTR_GLOBAL_HEAP)
  set(DEFINES_JERRY ${DEFINES_JERRY} JERRY_ATTR_GLOBAL_HEAP=${JERRY_ATTR_GLOBAL_HEAP})
endif()

# Memory usage limit for triggering garbage collection
if(JERRY_GC_LIMIT)
  set(DEFINES_JERRY ${DEFINES_JERRY} JERRY_GC_LIMIT=${JERRY_GC_LIMIT})
endif()

# Helper macro to set 0/1 switch as Jerry Defines
macro(jerry_add_define01 NAME)
  if(${NAME})
    set(DEFINES_JERRY ${DEFINES_JERRY} ${NAME}=1)
  else()
    set(DEFINES_JERRY ${DEFINES_JERRY} ${NAME}=0)
  endif()
endmacro(jerry_add_define01)

# Checks the optional features
# Enable 32 bit cpointers
jerry_add_define01(JERRY_CPOINTER_32_BIT)

# Fill error messages for builtin error objects
jerry_add_define01(JERRY_ERROR_MESSAGES)

# Use external context instead of static one
jerry_add_define01(JERRY_EXTERNAL_CONTEXT)

# JS-Parser
jerry_add_define01(JERRY_PARSER)

# JS line info
jerry_add_define01(JERRY_LINE_INFO)

# Logging
jerry_add_define01(JERRY_LOGGING)

# Memory statistics
jerry_add_define01(JERRY_MEM_STATS)

# Enable debugger
jerry_add_define01(JERRY_DEBUGGER)

# Memory management stress-test mode
jerry_add_define01(JERRY_MEM_GC_BEFORE_EACH_ALLOC)

# Parser byte-code dumps
jerry_add_define01(JERRY_PARSER_DUMP_BYTE_CODE)

# Profile
if (${JERRY_PROFILE} STREQUAL "es2015-subset")
  message(DEPRECATION "'es2015-subset' profile is deprecated, please use 'es.next' instead.")
endif()

if (NOT IS_ABSOLUTE ${JERRY_PROFILE})
  set(JERRY_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/profiles/${JERRY_PROFILE}.profile")
endif()

if(EXISTS ${JERRY_PROFILE})
  file(READ "${JERRY_PROFILE}" PROFILE_SETTINGS)
  string(REGEX REPLACE "[ \t]*#[^\n]*" "" PROFILE_SETTINGS "${PROFILE_SETTINGS}")
  string(REGEX REPLACE "[\r\n]" ";" PROFILE_SETTINGS "${PROFILE_SETTINGS}")

  # Process entries and save them as CMake variables.
  # This is required to correctly generate the jerryscript-config.h file.
  foreach(PROFILE_ENTRY ${PROFILE_SETTINGS})
    string(REPLACE "=" ";" PROFILE_ENTRY "${PROFILE_ENTRY}")
    list(GET PROFILE_ENTRY 0 PROFILE_KEY)
    list(GET PROFILE_ENTRY 1 PROFILE_VALUE)
    set(${PROFILE_KEY} ${PROFILE_VALUE})
  endforeach()

  set(DEFINES_JERRY ${DEFINES_JERRY} ${PROFILE_SETTINGS})
else()
  message(FATAL_ERROR "Profile file: '${JERRY_PROFILE}' doesn't exist!")
endif()

# Promise callback
jerry_add_define01(JERRY_PROMISE_CALLBACK)

# RegExp strict mode
jerry_add_define01(JERRY_REGEXP_STRICT_MODE)

# RegExp byte-code dumps
jerry_add_define01(JERRY_REGEXP_DUMP_BYTE_CODE)

# Snapshot exec
jerry_add_define01(JERRY_SNAPSHOT_EXEC)

# Snapshot save
jerry_add_define01(JERRY_SNAPSHOT_SAVE)

# Enable system allocator
jerry_add_define01(JERRY_SYSTEM_ALLOCATOR)

# Valgrind
jerry_add_define01(JERRY_VALGRIND)
if(JERRY_VALGRIND)
  set(INCLUDE_CORE_PRIVATE ${INCLUDE_CORE_PRIVATE} ${INCLUDE_THIRD_PARTY_VALGRIND})
endif()

# Enable VM execution stopping
jerry_add_define01(JERRY_VM_EXEC_STOP)

# Size of heap
set(DEFINES_JERRY ${DEFINES_JERRY} JERRY_GLOBAL_HEAP_SIZE=${JERRY_GLOBAL_HEAP_SIZE})

# Maximum size of stack memory usage
set(DEFINES_JERRY ${DEFINES_JERRY} JERRY_STACK_LIMIT=${JERRY_STACK_LIMIT})

# Maximum depth of recursion during GC mark phase
set(DEFINES_JERRY ${DEFINES_JERRY} JERRY_GC_MARK_LIMIT=${JERRY_GC_MARK_LIMIT})

## This function is to read "config.h" for default values
function(read_set_defines FILE PREFIX OUTPUTVAR)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}" INPUT_FILE_CONTENTS)

  # match all "#define <PREFIX>\n" lines
  # notes:
  #  * before the "#" there must be a newline and any number of spaces.
  #  * after the "#" there can be any number of spaces.
  string(REGEX MATCHALL "\r?\n[ ]*#[ ]*define ${PREFIX}[^\n]*"
         RAW_DEFINES "${INPUT_FILE_CONTENTS}")

  set(SELECTED_VARS )

  # Transform the defines to a list of (<name>; <value>; <name 2>; <value 2>; ...) list
  foreach(DEFINE_ENTRY ${RAW_DEFINES})
    # by default every define value is empty
    set(DEFINE_VALUE " ")

    # split up the define at the space between the define name and value (if there is any)

    # first remove "#define" part of the string
    string(REGEX REPLACE "\r?\n[ ]*#[ ]*define[ ]+" "" DEFINE_KEY_VALUE "${DEFINE_ENTRY}")
    string(FIND "${DEFINE_KEY_VALUE}" " " DEFINE_KEY_IDX)
    string(LENGTH "${DEFINE_KEY_VALUE}" DEFINE_LENGTH)

    if (DEFINE_KEY_IDX EQUAL "-1")
        set(DEFINE_KEY ${DEFINE_KEY_VALUE})
    else()
        string(SUBSTRING "${DEFINE_KEY_VALUE}" 0 ${DEFINE_KEY_IDX} DEFINE_KEY)
        string(SUBSTRING "${DEFINE_KEY_VALUE}" ${DEFINE_KEY_IDX} -1 DEFINE_VALUE)
        string(STRIP "${DEFINE_VALUE}" DEFINE_VALUE)
    endif()

    list(APPEND SELECTED_VARS ${DEFINE_KEY} ${DEFINE_VALUE})
  endforeach()

  set(${OUTPUTVAR} ${SELECTED_VARS} PARENT_SCOPE)
endfunction(read_set_defines)

# CONFIG_DEFAULTS contains define name and values which have the JERRY_ prefix
# as a list of (<name>; <value>; <name 2>; <value 2>; ...)
read_set_defines("config.h" JERRY_ CONFIG_DEFAULTS)


## Process the default values and build options to generate build config defines
list(LENGTH CONFIG_DEFAULTS CONFIG_DEFAULT_LENGTH)
math(EXPR CONFIG_DEFAULT_LENGTH "${CONFIG_DEFAULT_LENGTH} - 1")

set(JERRY_MODIFIED_OPTIONS)
foreach(CONFIG_IDX RANGE 0 ${CONFIG_DEFAULT_LENGTH} 2)
  list(GET CONFIG_DEFAULTS ${CONFIG_IDX} KEY)
  math(EXPR VALUE_IDX "${CONFIG_IDX} + 1")
  list(GET CONFIG_DEFAULTS ${VALUE_IDX} VALUE)

  # ${KEY} is the value for the given variable (aka define)
  # normalize ON/OFF cmake values to 1/0 for easier processing.
  if(${KEY} STREQUAL "ON")
    set(${KEY} 1)
  elseif(${KEY} STREQUAL "OFF")
    set(${KEY} 0)
  endif()

  # Generate "#define JERRY_<CONFIG> <CONFIG_VALUE>" entries if it is different from
  # the config default.

  # If the define loaded from the config file have a different value than the
  # relevant option passed for the CMake means that it does not have a default value.
  if(DEFINED ${KEY} AND NOT (${KEY} STREQUAL ${VALUE}))
    set(JERRY_MODIFIED_OPTIONS "${JERRY_MODIFIED_OPTIONS}#define ${KEY} ${${KEY}}\n")
  endif()
endforeach()

# Generate the jerryscript-config.h file into the build directory
# This file will contain the options different from the default (aka it's the build config).
if(JERRY_MODIFIED_OPTIONS)
  set(JERRY_BUILD_CFG
      "Generated differences from default by CMake based on build options:\n${JERRY_MODIFIED_OPTIONS}")
else()
  set(JERRY_BUILD_CFG "JerryScript configuration")
endif()
configure_file(config.h jerryscript-config.h @ONLY)

add_library(${JERRY_CORE_NAME} ${SOURCE_CORE_FILES})

target_compile_definitions(${JERRY_CORE_NAME} PUBLIC ${DEFINES_JERRY})
target_include_directories(${JERRY_CORE_NAME} PUBLIC ${INCLUDE_CORE_PUBLIC})
target_include_directories(${JERRY_CORE_NAME} PRIVATE ${INCLUDE_CORE_PRIVATE})

set(JERRY_CORE_PKGCONFIG_REQUIRES)
set(JERRY_CORE_PKGCONFIG_LIBS)
set(JERRY_CORE_PKGCONFIG_CFLAGS)

if(ENABLE_LTO)
  set(JERRY_CORE_PKGCONFIG_CFLAGS "${JERRY_CORE_PKGCONFIG_CFLAGS} -flto")
endif()

if(JERRY_MATH)
  target_link_libraries(${JERRY_CORE_NAME} jerry-math)
  set(JERRY_CORE_PKGCONFIG_REQUIRES libjerry-math)
else()
  if(USING_GCC OR USING_CLANG)
    set(CMAKE_REQUIRED_FLAGS "-Wno-error -Wno-error=strict-prototypes")
  endif()
  check_library_exists(m sin "" HAVE_M_LIB)
  if(HAVE_M_LIB)
    target_link_libraries(${JERRY_CORE_NAME} m)
    set(JERRY_CORE_PKGCONFIG_LIBS "${JERRY_CORE_PKGCONFIG_LIBS} -lm")
  endif()
endif()

separate_arguments(EXTERNAL_LINK_LIBS)
foreach(EXT_LIB ${EXTERNAL_LINK_LIBS})
  target_link_libraries(${JERRY_CORE_NAME} ${EXT_LIB})
  set(JERRY_CORE_PKGCONFIG_LIBS "${JERRY_CORE_PKGCONFIG_LIBS} -l${EXT_LIB}")
endforeach()

configure_file(libjerry-core.pc.in libjerry-core.pc @ONLY)

install(TARGETS ${JERRY_CORE_NAME} DESTINATION lib)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libjerry-core.pc DESTINATION lib/pkgconfig)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/jerryscript-config.h DESTINATION include)
install(DIRECTORY ${INCLUDE_CORE_PUBLIC}/ DESTINATION include)
