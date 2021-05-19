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
#include "DigitalOut-js.h"

/*
 * -- DigitalOut --
 * Public Member Functions:
 * 
 *  * DigitalOut (PinName pin)              -- Create a DigitalOut connected to the specified pin.
 *  * DigitalOut (PinName pin, int value)   -- Create a DigitalOut connected to the specified pin.
 *  * void 	write (int value)               -- Set the output, specified as 0 or 1 (int)
 *  * int 	read ()                         -- Return the output setting, represented as 0 or 1 (int)
 *  * int 	is_connected ()                 -- Return the output setting, represented as 0 or 1 (int)
*/

/**
 * DigitalOut#destructor
 *
 * Called when the DigitalOut is GC'ed
*/
void DigitalOut_destroy (void* ptr)
{
  delete static_cast<DigitalOut*>(ptr);
}

/**
 * Type infomation of the native DigitalOut pointer
 *
 * Set DigitalOut#destructor as the free callback.
 */
static const jerry_object_native_info_t native_obj_type_info =
{
  .free_cb = DigitalOut_destroy
};

/**
 * DigitalOut#write(int value)
 *
 * Writes binary value to DigitalOut.
 *
 * @param value 1/0 or true/false, specifies whether the output is high or low.
 *
 * @return undefined or an error (if invalid arguments are provided.)
 */
DECLARE_FUNCTION(write_handler)
{
  CHECK_ARGUMENT_COUNT(DigitalOut, write_handler, (args_cnt == 1));
  CHECK_ARGUMENT_TYPE_2(DigitalOut, write_handler, 0, number, boolean);

  void* ptr;

  if (!jerry_get_object_native_pointer (this_obj, &ptr, &native_obj_type_info))
  {
    LOG_PRINT ("Failed to get pointer property\n");
  }

  DigitalOut* native_ptr = static_cast<DigitalOut*>(ptr);
  jerry_value_t val = jerry_value_to_number(args_p[0]);
  int value = static_cast<int>(jerry_get_number_value (val));
  jerry_release_value (val);
  native_ptr->write (value);
  return jerry_create_undefined ();
}

/**
 * DigitalOut#read()
 *
 * Reads current status of DigitalOut
 *
 * @return 1 if the pin is high, or 0 if the pin is low
*/
DECLARE_FUNCTION(read_handler)
{
  CHECK_ARGUMENT_COUNT(DigitalOut, read_handler, (args_cnt == 0));
  void* ptr;

  if (!jerry_get_object_native_pointer (this_obj, &ptr, &native_obj_type_info))
  {
    LOG_PRINT ("Failed to get pointer property\n");
  }

  DigitalOut* native_ptr = static_cast<DigitalOut*>(ptr);
  int result = native_ptr->read ();
  return jerry_create_number (result);
}

/**
 * DigitalOut#is_connected()
 *
 * @returns 0 if the DigitalOut is set to NC (Not Connected),
 * or 1 if it is connected to an actual pin.
*/
DECLARE_FUNCTION(is_connected_handler)
{
  CHECK_ARGUMENT_COUNT(DigitalOut, is_connected_handler, (args_cnt == 0));
  void* ptr;

  if (!jerry_get_object_native_pointer (this_obj, &ptr, &native_obj_type_info))
  {
    LOG_PRINT ("Failed to get pointer property\n");
  }

  DigitalOut* native_ptr = static_cast<DigitalOut*>(ptr);
  int result = native_ptr->is_connected ();
  return jerry_create_number (result);
}

 /**
 * DigitalOut#constructor
 *
 * @param pin_name mbed pin to connect the DigitalOut.
 * @param value (optional) initial value for the DigitalOut.
 * @returns a JS object representing DigitalOut.
 */
DECLARE_FUNCTION(DigitalOut_handler)
{
  CHECK_ARGUMENT_COUNT(DigitalOut, constructor, (args_cnt == 1 || args_cnt == 2));
  CHECK_ARGUMENT_TYPE(DigitalOut, constructor, 0, number);
  CHECK_ARGUMENT_TYPE_ON_CONDITION(DigitalOut, constructor, 1, number, (args_cnt == 2));

  DigitalOut* native_ptr;

  PinName pin = PinName (jerry_get_number_value (args_p[0]));

  switch (args_cnt)
  {
  case 1:
    native_ptr = new DigitalOut (pin);
    break;
  case 2:
    int value = static_cast<int>(jerry_get_number_value (args_p[1]));
    native_ptr = new DigitalOut (pin, value);
    break;
  }

  jerry_set_object_native_pointer (this_obj, native_ptr, &native_obj_type_info);
  return jerry_create_undefined ();
}

/**
 * register_DigitalOut
 *
 * Register handlers to DigitalOut's prototype object and add DigitalOut to global
*/
void register_DigitalOut ()
{
  jerry_value_t func_obj = jerry_create_external_function (DigitalOut_handler);

  /*Create prototype*/
  jerry_value_t Proto_obj = jerry_create_object ();

  //*Add handlers to prototype
  register_handler (Proto_obj, is_connected_handler, "is_connected");
  register_handler (Proto_obj, read_handler, "read");
  register_handler (Proto_obj, write_handler, "write");

  /*Add prototype property to DigitalOut's function_object*/
  register_object (func_obj, Proto_obj, "prototype");

  jerry_release_value (Proto_obj);

  /*Register to global*/
  register_object_to_global (func_obj, "DigitalOut");
  jerry_release_value (func_obj);
}
