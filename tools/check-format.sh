#!/bin/bash

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

JERRY_CORE_FILES=`find ./jerry-core -name "*.c" -or -name "*.h"`
JERRY_EXT_FILES=`find ./jerry-ext -name "*.c" -or -name "*.h"`
JERRY_PORT_FILES=`find ./jerry-port -name "*.c" -or -name "*.h"`
JERRY_MATH_FILES=`find ./jerry-math -name "*.c" -or -name "*.h"`
JERRY_MAIN_FILES=`find ./jerry-main -name "*.c" -or -name "*.h"`
UNIT_TEST_FILES=`find ./tests/unit-* -name "*.c" -or -name "*.h"`

OPTIONS="--dry-run --Werror"

if [ -n "$1" ]
then
  if [ "$1" == "--fix" ] || [ "$1" == "-f" ]
  then
    OPTIONS="-i"
  else
    MANUAL_CHECK_FILES=`find $1 -name "*.c" -or -name "*.h"`
  fi
fi

clang-format-10 $OPTIONS \
 $MANUAL_CHECK_FILES \
 $JERRY_CORE_FILES \
 $JERRY_EXT_FILES \
 $JERRY_PORT_FILES \
 $JERRY_MATH_FILES \
 $JERRY_MAIN_FILES \
 $UNIT_TEST_FILES \
