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
#include "mbed.h"
#include "rtos.h"

#include "jerry-core/include/jerryscript.h"
#include "jerry-core/include/jerryscript-port.h"

#include "jerryscript-mbed-event-loop/EventLoop.h"

#include "jerryscript-mbed-util/js_source.h"
#include "jerryscript-mbed-library-registry/registry.h"

#include "jerryscript-mbed-launcher/launcher.h"
#include "jerryscript-mbed-launcher/setup.h"

#include "jerry-targetjs.h"

DECLARE_JS_CODES;

/**
 * load_javascript
 *
 * Parse and run javascript files specified in jerry-targetjs.h
 */
static int load_javascript() {
    for (int src = 0; js_codes[src].source; src++) {
        LOG_PRINT("running js file %s\r\n", js_codes[src].name);

        const jerry_char_t* code = reinterpret_cast<const jerry_char_t*>(js_codes[src].source);
        const size_t length = js_codes[src].length;

        jerry_value_t parsed_code = jerry_parse(NULL, 0, code, length, JERRY_PARSE_NO_OPTS);

        if (jerry_value_is_error(parsed_code)) {
            LOG_PRINT_ALWAYS("jerry_parse failed [%s]\r\n", js_codes[src].name);
            jerry_release_value(parsed_code);
            jsmbed_js_exit();
            return -1;
        }

        jerry_value_t returned_value = jerry_run(parsed_code);
        jerry_release_value(parsed_code);

        if (jerry_value_is_error(returned_value)) {
            LOG_PRINT_ALWAYS("jerry_run failed [%s]\r\n", js_codes[src].name);
            jerry_release_value(returned_value);
            jsmbed_js_exit();
            return -1;
        }

        jerry_release_value(returned_value);
    }

    return 0;
}

int jsmbed_js_init() {
    srand ((unsigned) jerry_port_get_current_time());
    jerry_init_flag_t flags = JERRY_INIT_EMPTY;
    jerry_init(JERRY_INIT_FLAGS (flags));

    jsmbed_js_load_magic_strings();
    mbed::js::LibraryRegistry::getInstance().register_all();

    return 0;
}

void jsmbed_js_exit() {
    jerry_cleanup();
}

void jsmbed_js_launch() {
    jsmbed_js_init();

    puts("   JerryScript in mbed\r\n");
    puts("   build date:  " __DATE__ " \r\n");

    if (load_javascript() == 0) {
        mbed::js::event_loop();
    }
}
