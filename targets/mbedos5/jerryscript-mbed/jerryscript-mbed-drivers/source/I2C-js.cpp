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
#include "jerryscript-mbed-util/logging.h"
#include "jerryscript-mbed-drivers/I2C-js.h"
#include "jerryscript-mbed-library-registry/wrap_tools.h"

#include "mbed.h"

/**
 * I2C#destructor
 *
 * Called if/when the I2C object is GC'ed.
 */
void NAME_FOR_CLASS_NATIVE_DESTRUCTOR(I2C) (void *void_ptr, jerry_object_native_info_t *info_p) {
    (void) info_p;
    delete static_cast<I2C*>(void_ptr);
}

/**
 * Type infomation of the native I2C pointer
 *
 * Set I2C#destructor as the free callback.
 */
static const jerry_object_native_info_t native_obj_type_info = {
    .free_cb = NAME_FOR_CLASS_NATIVE_DESTRUCTOR(I2C)
};

/**
 * I2C#frequency (native JavaScript method)
 *
 * Set the frequency of the I2C bus.
 *
 * @param frequency New I2C Frequency
 */
DECLARE_CLASS_FUNCTION(I2C, frequency) {
    CHECK_ARGUMENT_COUNT(I2C, frequency, (args_count == 1));
    CHECK_ARGUMENT_TYPE_ALWAYS(I2C, frequency, 0, number);

    // Unwrap native I2C object
    void *void_ptr;
    bool has_ptr = jerry_get_object_native_pointer(call_info_p->this_value, &void_ptr, &native_obj_type_info);

    if (!has_ptr) {
        return jerry_create_error(JERRY_ERROR_TYPE,
                                  (const jerry_char_t *) "Failed to get native I2C pointer");
    }

    I2C *native_ptr = static_cast<I2C*>(void_ptr);

    int hz = jerry_get_number_value(args[0]);
    native_ptr->frequency(hz);

    return jerry_create_undefined();
}

/**
 * I2C#read (native JavaScript method)
 *
 * Read data from the I2C bus.
 *
 * @overload I2C#read(int)
 * Read a single byte from the I2C bus
 *
 * @param ack indicates if the byte is to be acknowledged (1 => acknowledge)
 *
 * @returns array: Data read from the I2C bus
 *
 * @overload I2C#read(int, array, int)
 * Read a series of bytes from the I2C bus
 *
 * @param address I2C address to read from
 * @param data Array to read into
 * @param length Length of data to read
 *
 * @returns array: Data read from the I2C bus
 */
DECLARE_CLASS_FUNCTION(I2C, read) {
    CHECK_ARGUMENT_COUNT(I2C, read, (args_count == 1 || args_count == 3 || args_count == 4));

    if (args_count == 1) {
        CHECK_ARGUMENT_TYPE_ALWAYS(I2C, read, 0, number);
        void *void_ptr;
        bool has_ptr = jerry_get_object_native_pointer(call_info_p->this_value, &void_ptr, &native_obj_type_info);

        if (!has_ptr) {
            return jerry_create_error(JERRY_ERROR_TYPE,
                                      (const jerry_char_t *) "Failed to get native I2C pointer");
        }

        I2C *native_ptr = static_cast<I2C*>(void_ptr);

        int data = jerry_get_number_value(args[0]);
        int result = native_ptr->read(data);

        return jerry_create_number(result);
    } else {
        CHECK_ARGUMENT_TYPE_ALWAYS(I2C, read, 0, number);
        CHECK_ARGUMENT_TYPE_ALWAYS(I2C, read, 1, array);
        CHECK_ARGUMENT_TYPE_ALWAYS(I2C, read, 2, number);

        CHECK_ARGUMENT_TYPE_ON_CONDITION(I2C, read, 3, boolean, (args_count == 4));

        void *void_ptr;
        bool has_ptr = jerry_get_object_native_pointer(call_info_p->this_value, &void_ptr, &native_obj_type_info);

        if (!has_ptr) {
            return jerry_create_error(JERRY_ERROR_TYPE,
                                      (const jerry_char_t *) "Failed to get native I2C pointer");
        }

        I2C *native_ptr = static_cast<I2C*>(void_ptr);

        const uint32_t data_len = jerry_get_array_length(args[1]);

        int address = jerry_get_number_value(args[0]);
        int length = jerry_get_number_value(args[2]);

        char *data = new char[data_len];

        bool repeated = false;
        if (args_count == 4) {
            repeated = jerry_value_is_true(args[3]);
        }

        int result = native_ptr->read(address, data, length, repeated);

        jerry_value_t out_array = jerry_create_array(data_len);

        for (uint32_t i = 0; i < data_len; i++) {
            jerry_value_t val = jerry_create_number(double(data[i]));
            jerry_release_value(jerry_set_property_by_index(out_array, i, val));
            jerry_release_value(val);
        }

        delete[] data;

        if (result == 0) {
            // ACK
            return out_array;
        } else {
            // NACK
            const char *error_msg = "NACK received from I2C bus";

            jerry_release_value(out_array);
            return jerry_create_error(JERRY_ERROR_COMMON, reinterpret_cast<const jerry_char_t *>(error_msg));
        }
    }
}

/**
 * I2C#write (native JavaScript method)
 *
 * @overload I2C#write(int)
 * Write a single byte to the I2C bus
 *
 * @param data Data byte to write to the I2C bus
 *
 * @returns 1 on success, 0 on failure
 *
 * @overload I2C#write(int, array, int, bool)
 * Write an array of data to a certain address on the I2C bus
 *
 * @param address 8-bit I2C slave address
 * @param data Array of bytes to send
 * @param length Length of data to write
 * @param repeated (optional) If true, do not send stop at end.
 *
 * @returns 0 on success, non-0 on failure
 */
DECLARE_CLASS_FUNCTION(I2C, write) {
    CHECK_ARGUMENT_COUNT(I2C, write, (args_count == 1 || args_count == 3 || args_count == 4));

    if (args_count == 1) {
        CHECK_ARGUMENT_TYPE_ALWAYS(I2C, write, 0, number);

        // Extract native I2C object
        void *void_ptr;
        bool has_ptr = jerry_get_object_native_pointer(call_info_p->this_value, &void_ptr, &native_obj_type_info);

        if (!has_ptr) {
            return jerry_create_error(JERRY_ERROR_TYPE,
                                      (const jerry_char_t *) "Failed to get native I2C pointer");
        }

        I2C *native_ptr = static_cast<I2C*>(void_ptr);

        // Unwrap arguments
        int data = jerry_get_number_value(args[0]);

        int result = native_ptr->write(data);
        return jerry_create_number(result);
    } else {
        // 3 or 4
        CHECK_ARGUMENT_TYPE_ALWAYS(I2C, write, 0, number);
        CHECK_ARGUMENT_TYPE_ALWAYS(I2C, write, 1, array);
        CHECK_ARGUMENT_TYPE_ALWAYS(I2C, write, 2, number);
        CHECK_ARGUMENT_TYPE_ON_CONDITION(I2C, write, 3, boolean, (args_count == 4));

        // Extract native I2C object
        void *void_ptr;
        bool has_ptr = jerry_get_object_native_pointer(call_info_p->this_value, &void_ptr, &native_obj_type_info);

        if (!has_ptr) {
            return jerry_create_error(JERRY_ERROR_TYPE,
                                      (const jerry_char_t *) "Failed to get native I2C pointer");
        }

        I2C *native_ptr = static_cast<I2C*>(void_ptr);

        // Unwrap arguments
        int address = jerry_get_number_value(args[0]);
        const uint32_t data_len = jerry_get_array_length(args[1]);
        int length = jerry_get_number_value(args[2]);
        bool repeated = args_count == 4 && jerry_value_is_true(args[3]);

        // Construct data byte array
        char *data = new char[data_len];
        for (uint32_t i = 0; i < data_len; i++) {
            data[i] = jerry_get_number_value(jerry_get_property_by_index(args[1], i));
        }

        int result = native_ptr->write(address, data, length, repeated);

        // free dynamically allocated resources
        delete[] data;

        return jerry_create_number(result);
    }
}

/**
 * I2C#start (native JavaScript method)
 *
 * Creates a start condition on the I2C bus.
 */
DECLARE_CLASS_FUNCTION(I2C, start) {
    CHECK_ARGUMENT_COUNT(I2C, start, (args_count == 0));

    // Extract native I2C object
    void *void_ptr;
    bool has_ptr = jerry_get_object_native_pointer(call_info_p->this_value, &void_ptr, &native_obj_type_info);

    if (!has_ptr) {
        return jerry_create_error(JERRY_ERROR_TYPE,
                                  (const jerry_char_t *) "Failed to get native I2C pointer");
    }

    I2C *native_ptr = static_cast<I2C*>(void_ptr);

    native_ptr->start();
    return jerry_create_undefined();
}

/**
 * I2C#stop (native JavaScript method)
 *
 * Creates a stop condition on the I2C bus.
 */
DECLARE_CLASS_FUNCTION(I2C, stop) {
    CHECK_ARGUMENT_COUNT(I2C, stop, (args_count == 0));

    // Extract native I2C object
    void *void_ptr;
    bool has_ptr = jerry_get_object_native_pointer(call_info_p->this_value, &void_ptr, &native_obj_type_info);

    if (!has_ptr) {
        return jerry_create_error(JERRY_ERROR_TYPE,
                                  (const jerry_char_t *) "Failed to get native I2C pointer");
    }

    I2C *native_ptr = static_cast<I2C*>(void_ptr);

    native_ptr->stop();
    return jerry_create_undefined();
}

/**
 * I2C (native JavaScript constructor)
 *
 * @param sda mbed pin for I2C data
 * @param scl mbed pin for I2C clock
 * @returns a JavaScript object representing the I2C bus.
 */
DECLARE_CLASS_CONSTRUCTOR(I2C) {
    CHECK_ARGUMENT_COUNT(I2C, __constructor, (args_count == 2));
    CHECK_ARGUMENT_TYPE_ALWAYS(I2C, __constructor, 0, number);
    CHECK_ARGUMENT_TYPE_ALWAYS(I2C, __constructor, 1, number);

    int sda = jerry_get_number_value(args[0]);
    int scl = jerry_get_number_value(args[1]);

    I2C *native_ptr = new I2C((PinName)sda, (PinName)scl);

    jerry_value_t js_object = jerry_create_object();
    jerry_set_object_native_pointer(js_object, native_ptr, &native_obj_type_info);

    ATTACH_CLASS_FUNCTION(js_object, I2C, frequency);
    ATTACH_CLASS_FUNCTION(js_object, I2C, read);
    ATTACH_CLASS_FUNCTION(js_object, I2C, write);
    ATTACH_CLASS_FUNCTION(js_object, I2C, start);
    ATTACH_CLASS_FUNCTION(js_object, I2C, stop);

    return js_object;
}
