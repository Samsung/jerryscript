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

MAGIC_STRINGS_GEN="tools/gen-magic-strings.py"
MAGIC_STRINGS_INC_H="jerry-core/lit/lit-magic-strings.inc.h"
FORMAT_MAGIC_STRINGS="clang-format-10 --i "$MAGIC_STRINGS_INC_H
MAGIC_STRINGS_TEMP=`mktemp lit-magic-strings.inc.h.XXXXXXXXXX`

cp $MAGIC_STRINGS_INC_H $MAGIC_STRINGS_TEMP
$MAGIC_STRINGS_GEN
DIFF_RESULT=$?
$FORMAT_MAGIC_STRINGS

if [ $DIFF_RESULT -eq 0 ]
then
  diff -q $MAGIC_STRINGS_INC_H $MAGIC_STRINGS_TEMP
  DIFF_RESULT=$?
  if [ $DIFF_RESULT -ne 0 ]
  then
    echo -e "\e[1;33m$MAGIC_STRINGS_INC_H must be re-generated. Run $MAGIC_STRINGS_GEN\e[0m"
  fi
fi
mv $MAGIC_STRINGS_TEMP $MAGIC_STRINGS_INC_H

exit $DIFF_RESULT
