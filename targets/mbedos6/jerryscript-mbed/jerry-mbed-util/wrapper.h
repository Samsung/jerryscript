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
#ifndef WRAPPER_H
#define WRAPPER_H

#include "jerryscript.h"
#include "logging.h"

/*Function declaration macro*/
#define DECLARE_FUNCTION(NAME) \
static jerry_value_t NAME(const jerry_value_t func_obj, \
                          const jerry_value_t this_obj,\
                          const jerry_value_t *args_p, \
                          const jerry_length_t args_cnt) \

/*Argument checkking macros*/
#define CHECK_ARGUMENT_COUNT(CLASS, NAME, EXPR) \
  if (!(EXPR)) \
  { \
    const char* error_msg = "ERROR: wrong argument count for " # CLASS "." # NAME ", expected " # EXPR "."; \
    return jerry_create_error (JERRY_ERROR_TYPE, reinterpret_cast<const jerry_char_t*>(error_msg)); \
  }
/*Check argument type on the given index*/
#define CHECK_ARGUMENT_TYPE(CLASS, NAME, INDEX, TYPE) \
  if (!jerry_value_is_ ## TYPE (args_p[INDEX])) \
  { \
    const char* error_msg = "ERROR: wrong argument type for " # CLASS "." # NAME ", expected argument " # INDEX " to be a " # TYPE ".\n"; \
    return jerry_create_error (JERRY_ERROR_TYPE, reinterpret_cast<const jerry_char_t*>(error_msg)); \
  }
/*Check argument type on the given index -- checks for two types like 'int or boolean'*/
#define CHECK_ARGUMENT_TYPE_2(CLASS, NAME, INDEX, TYPE, TYPE2) \
  if (!jerry_value_is_ ## TYPE (args_p[INDEX]) && !jerry_value_is_ ## TYPE2 (args_p[INDEX])) \
  { \
    const char* error_msg = "ERROR: wrong argument type for " # CLASS "." # NAME ", expected argument " # INDEX " to be a " # TYPE "or " # TYPE2".\n"; \
    return jerry_create_error (JERRY_ERROR_TYPE, reinterpret_cast<const jerry_char_t*>(error_msg)); \
  }

#define CHECK_ARGUMENT_TYPE_ON_CONDITION(CLASS, NAME, INDEX, TYPE, EXPR) \
  if ((EXPR)) \
  { \
    if (!jerry_value_is_ ## TYPE (args_p[INDEX])) \
    { \
      const char* error_msg = "ERROR: wrong argument type for " # CLASS "." # NAME ", expected argument " # INDEX " to be a " # TYPE ".\n"; \
      return jerry_create_error (JERRY_ERROR_TYPE, reinterpret_cast<const jerry_char_t*>(error_msg)); \
    } \
  }

/**
 * Register function to object macro *
 *
 * Registers exterenal functions to the given object
 * @param this_obj register the function to this object
 * @param handler  fucntion that we want to register
 * @param name     name property, that will be called in JS
 * @return return jerry boolean(if no error happened) or jerry error
 *
 * @note errors are not handled in this function
 */
inline void register_handler (jerry_value_t this_obj, jerry_external_handler_t handler, const char* name)
{
  jerry_value_t native_func = jerry_create_external_function (handler);
  jerry_value_t prop_name = jerry_create_string (reinterpret_cast<const jerry_char_t*>(name));
  jerry_release_value (jerry_set_property (this_obj, prop_name, native_func));
  jerry_release_value (native_func);
  jerry_release_value (prop_name);
}

/**
 * Register object to global macro *
 *
 * Registers the given object to global
 * @param obj      object to register
 * @param name     name property, that will be called in JS
 *
 * TODO: 
 * Handle errors if needed
 */
inline void register_object_to_global (jerry_value_t obj, const char* name)
{
  jerry_value_t global_obj = jerry_get_global_object ();
  jerry_value_t prop_name = jerry_create_string (reinterpret_cast<const jerry_char_t*>(name));
  jerry_release_value (jerry_set_property (global_obj, prop_name, obj));
  jerry_release_value (prop_name);
  jerry_release_value (global_obj);
}

/**
 * Register object to object macro *
 *
 * Registers the given object (obj) to an other object (this_obj)
 * @param this_obj object to register to
 * @param obj      object to register
 * @param name     name property, that will be called in JS
 *
 * TODO:
 * Handle errors if needed
 */
inline void register_object (jerry_value_t this_obj, jerry_value_t obj, const char* name)
{
  jerry_value_t prop_name = jerry_create_string (reinterpret_cast<const jerry_char_t*>(name));
  jerry_release_value (jerry_set_property (this_obj, prop_name, obj));
  jerry_release_value (prop_name);
}

#endif //WRAPPER_H
