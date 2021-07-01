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
project (jerry-main C)

# Optional build settings
set(ENABLE_LINK_MAP           OFF CACHE BOOL "Enable generating a link map file?")
set(JERRY_TEST_STACK_MEASURE  OFF CACHE BOOL "Enable stack measurement for the jerry-test binary?")

# Status messages
message(STATUS "ENABLE_LINK_MAP                " ${ENABLE_LINK_MAP})
message(STATUS "JERRY_TEST_STACK_MEASURE       " ${JERRY_TEST_STACK_MEASURE})

# Generate map file
if(ENABLE_LINK_MAP)
  if("${PLATFORM}" STREQUAL "DARWIN")
    set(LINKER_FLAGS_COMMON "${LINKER_FLAGS_COMMON} -Xlinker -map -Xlinker jerry.map")
  else()
    set(LINKER_FLAGS_COMMON "${LINKER_FLAGS_COMMON} -Xlinker -Map -Xlinker jerry.map")
  endif()
endif()

# Get version information from git
if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
  execute_process(COMMAND git rev-parse --short HEAD
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE JERRY_COMMIT_HASH
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(JERRY_COMMIT_HASH " (${JERRY_COMMIT_HASH})")
else()
  set(JERRY_COMMIT_HASH "")
endif()

set(DEFINES_JERRY ${DEFINES_JERRY} JERRY_COMMIT_HASH="${JERRY_COMMIT_HASH}")

macro(jerry_create_executable JERRY_NAME)
  add_executable(${JERRY_NAME} ${ARGN})
  set_property(TARGET ${JERRY_NAME}
               PROPERTY LINK_FLAGS "${LINKER_FLAGS_COMMON}")
  target_compile_definitions(${JERRY_NAME} PRIVATE ${DEFINES_JERRY})

  target_link_libraries(${JERRY_NAME} jerry-core)

  install(TARGETS ${JERRY_NAME} DESTINATION bin)
endmacro()

# Jerry with libfuzzer support
if(JERRY_LIBFUZZER)
  jerry_create_executable("jerry-libfuzzer" "libfuzzer.c")
  if(JERRY_OSS_FUZZ)
    target_link_libraries("jerry-libfuzzer" jerry-port-default)
  else()
    target_link_libraries("jerry-libfuzzer" jerry-port-default -fsanitize=fuzzer)
  endif()
endif()

# Jerry standalones
if(JERRY_CMDLINE)
  jerry_create_executable("jerry" "main-jerry.c" "main-utils.c" "main-options.c" "cli.c")
  target_link_libraries("jerry" jerry-ext jerry-port-default)
endif()

if(JERRY_CMDLINE_TEST)
  jerry_create_executable("jerry-test" "main-jerry-test.c" "benchmarking.c")
  target_link_libraries("jerry-test" jerry-port-default)
  if (JERRY_TEST_STACK_MEASURE)
    target_compile_definitions("jerry-test" PRIVATE -DJERRY_TEST_STACK_MEASURE=1)
  endif()
endif()

if(JERRY_CMDLINE_SNAPSHOT)
  jerry_create_executable("jerry-snapshot" "main-jerry-snapshot.c" "cli.c")
  target_link_libraries("jerry-snapshot" jerry-port-default)
endif()
