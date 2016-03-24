/* Copyright 2014-2015 Samsung Electronics Co., Ltd.
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

#include "mbed-drivers/mbed.h"

#include "jerry-core/jerry.h"
#include "jerry_run.h"

//-----------------------------------------------------------------------------

#include "jerry_targetjs.h"


static int jerry_init(void) {
  int retcode,src;

  DECLARE_JS_CODES;


  printf("run main.js:  ");
  /* run main.js */
  retcode = js_entry(js_codes[0].source, js_codes[0].length);
  if (retcode != 0) {
    printf("js_entry failed code(%d) [%s]\r\n", retcode, js_codes[0].name);
    js_exit();
    return -1;
  }

  printf("run rest of the js files:  ");
  /* run rest of the js files */
    printf("<<<<THE TEST ..>>>>");
    for (src=1; js_codes[src].source; src++) {
      retcode = js_eval(js_codes[src].source, js_codes[src].length);
      if (retcode != 0) {
        printf("js_eval failed code(%d) [%s]\r\n", retcode, js_codes[src].name);
        js_exit();
        return -2;
      }
    }
  printf("<<<< END OF THE BENCHMARK TESTING >>>>>\n\r"); 
  return 0;
}

static void jerry_loop(void) {
  static uint32_t _jcount = 0;

  js_loop(_jcount++);
}


void app_start(int, char**){

  // set 115200 baud rate for stdout
  static Serial pc(USBTX, USBRX); //tx, rx
  pc.baud(9600);

  printf ("\r\nJerryScript in ST32 Nucleo F4 board.\r\n");
  //printf ("   build  %s\r\n", jerry_build_date);
  //printf ("   hash   %s\r\n", jerry_commit_hash);
  //printf ("   branch %s\r\n", jerry_branch_name);

  if (jerry_init() == 0) {
    //minar::Scheduler::postCallback(jerry_loop).period(minar::milliseconds(100));
    jerry_loop();
    js_exit();
  }
}
