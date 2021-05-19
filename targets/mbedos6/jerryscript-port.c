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
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <jerryscript-port.h>

/**
 * Provide log message implementation for the engine.
 */
void
jerry_port_log (jerry_log_level_t level, /**< log level */
                const char *format, /**< format string */
                ...)  /**< parameters */
{
  va_list args;
  va_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);

  if (strlen (format) == 1 && format[0] == 0x0a)
  {
    printf ("\r");
  }
}

/**
 * Print a character to stdout with putchar.
 */
void
jerry_port_print_char (char c)
{
  putchar (c);
}

/**
 * Default implementation of jerry_port_get_local_time_zone_adjustment.
 */
double jerry_port_get_local_time_zone_adjustment (double unix_ms, /**< ms since unix epoch */
                                                  bool is_utc)    /**< is the time above in UTC? */
{
  return 0;
}

/**
 * Default implementation of jerry_port_get_current_time.
 */
double jerry_port_get_current_time (void)
{
  struct timeval tv;

  if (gettimeofday (&tv, NULL) != 0)
  {
    return 0;
  }

  return ((double) tv.tv_sec) * 1000.0 + ((double) tv.tv_usec) / 1000.0;
}

/**
 * Default implementation of jerry_port_fatal. Calls 'abort' if exit code is
 * non-zero, 'exit' otherwise.
 */
void jerry_port_fatal (jerry_fatal_code_t code) /**< cause of error */
{
  if (code != 0
      && code != ERR_OUT_OF_MEMORY)
  {
    abort ();
  }

  exit ((int) code);
}

/*Functions for jerry-ext*/

/**
 * Determines the size of the given file.
 *
 * @return size of the file
 */
static size_t
jerry_port_get_file_size (FILE *file_p) /**< opened file */
{
  fseek (file_p, 0, SEEK_END);
  long size = ftell (file_p);
  fseek (file_p, 0, SEEK_SET);

  return (size_t) size;
}

/**
 * Opens file with the given path and reads its source.
 *
 * @return the source of the file
 */
uint8_t *
jerry_port_read_source (const char *file_name_p, /**< file name */
                        size_t *out_size_p) /**< [out] read bytes */
{
  FILE *file_p = fopen (file_name_p, "rb");

  if (file_p == NULL)
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: Failed to open file: %s\n", file_name_p);
    return NULL;
  }

  size_t file_size = jerry_port_get_file_size (file_p);
  uint8_t *buffer_p = (uint8_t *) malloc (file_size);

  if (buffer_p == NULL)
  {
    fclose (file_p);

    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: Failed to allocate memory for file: %s\n", file_name_p);
    return NULL;
  }

  size_t bytes_read = fread (buffer_p, 1u, file_size, file_p);

  if (bytes_read != file_size)
  {
    fclose (file_p);
    free (buffer_p);

    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: Failed to read file: %s\n", file_name_p);
    return NULL;
  }

  fclose (file_p);
  *out_size_p = bytes_read;

  return buffer_p;
}

/**
 * Release the previously opened file's content.
 */
void
jerry_port_release_source (uint8_t *buffer_p) /**< buffer to free */
{
  free (buffer_p);
}

/**
 * Normalize a file path
 *
 * @return length of the path written to the output buffer
 */
size_t
jerry_port_normalize_path (const char *in_path_p,   /**< input file path */
                           char *out_buf_p,         /**< output buffer */
                           size_t out_buf_size,     /**< size of output buffer */
                           char *base_file_p)       /**< base file path */
{
  size_t ret = 0;

#if defined (WIN32)
  char drive[_MAX_DRIVE];
  char *dir_p = (char *) malloc (_MAX_DIR);

  char *path_p = (char *) malloc (_MAX_PATH * 2);
  *path_p = '\0';

  if (base_file_p != NULL)
  {
    _splitpath_s (base_file_p,
                  &drive,
                  _MAX_DRIVE,
                  dir_p,
                  _MAX_DIR,
                  NULL,
                  0,
                  NULL,
                  0);
    strncat (path_p, &drive, _MAX_DRIVE);
    strncat (path_p, dir_p, _MAX_DIR);
  }

  strncat (path_p, in_path_p, _MAX_PATH);

  char *norm_p = _fullpath (out_buf_p, path_p, out_buf_size);

  free (path_p);
  free (dir_p);

  if (norm_p != NULL)
  {
    ret = strnlen (norm_p, out_buf_size);
  }
#elif defined (__unix__) || defined (__APPLE__)
#define MAX_JERRY_PATH_SIZE 256
  char *buffer_p = (char *) malloc (MAX_JERRY_PATH_SIZE);
  char *path_p = (char *) malloc (MAX_JERRY_PATH_SIZE);

  char *base_p = dirname (base_file_p);
  strncpy (path_p, base_p, MAX_JERRY_PATH_SIZE);
  strncat (path_p, "/", 1);
  strncat (path_p, in_path_p, MAX_JERRY_PATH_SIZE);

  char *norm_p = realpath (path_p, buffer_p);
  free (path_p);

  if (norm_p != NULL)
  {
    const size_t len = strnlen (norm_p, out_buf_size);
    if (len < out_buf_size)
    {
      strncpy (out_buf_p, norm_p, out_buf_size);
      ret = len;
    }
  }

  free (buffer_p);
#undef MAX_JERRY_PATH_SIZE
#else
  (void) base_file_p;

  /* Do nothing, just copy the input. */
  const size_t len = strnlen (in_path_p, out_buf_size);
  if (len < out_buf_size)
  {
    strncpy (out_buf_p, in_path_p, out_buf_size);
    ret = len;
  }
#endif

  return ret;
} /* jerry_port_normalize_path */

/**
 * Get the module object of a native module.
 *
 * @return Undefined, if 'name' is not a native module
 *         jerry_value_t containing the module object, otherwise
 */
jerry_value_t
jerry_port_get_native_module (jerry_value_t name) /**< module specifier */
{
  (void) name;
  return jerry_create_undefined ();
}

#ifndef DISABLE_EXTRA_API

/**
 * Actual log level
 */
static jerry_log_level_t jerry_port_default_log_level = JERRY_LOG_LEVEL_ERROR;

#define JERRY_PORT_DEFAULT_LOG_LEVEL jerry_port_default_log_level

/**
 * Get the log level
 *
 * @return current log level
 *
 * Note:
 *      This function is only available if the port implementation library is
 *      compiled without the DISABLE_EXTRA_API macro.
 */
jerry_log_level_t
jerry_port_default_get_log_level (void)
{
  return jerry_port_default_log_level;
}

/**
 * Set the log level
 *
 * Note:
 *      This function is only available if the port implementation library is
 *      compiled without the DISABLE_EXTRA_API macro.
 */
void
jerry_port_default_set_log_level (jerry_log_level_t level) /**< log level */
{
  jerry_port_default_log_level = level;
}

#else /* DISABLE_EXTRA_API */
#define JERRY_PORT_DEFAULT_LOG_LEVEL JERRY_LOG_LEVEL_ERROR
#endif /* !DISABLE_EXTRA_API */
