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

#if !defined(_WIN32)
#include <libgen.h>
#endif /* !defined (_WIN32) */
#include "jerryscript-port-default.h"
#include "jerryscript-port.h"
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) &S_IFMT) == S_IFDIR)
#endif

/**
 * Determines the size of the given file.
 * @return size of the file
 */
static size_t
jerry_port_get_file_size (FILE *file_p) /**< opened file */
{
  fseek (file_p, 0, SEEK_END);
  long size = ftell (file_p);
  fseek (file_p, 0, SEEK_SET);

  return (size_t) size;
} /* jerry_port_get_file_size */

/**
 * Opens file with the given path and reads its source.
 * @return the source of the file
 */
uint8_t *
jerry_port_read_source (const char *file_name_p, /**< file name */
                        size_t *out_size_p) /**< [out] read bytes */
{
  struct stat stat_buffer;
  if (stat (file_name_p, &stat_buffer) == -1 || S_ISDIR (stat_buffer.st_mode))
  {
    jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: Failed to open file: %s\n", file_name_p);
    return NULL;
  }

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
} /* jerry_port_read_source */

/**
 * Release the previously opened file's content.
 */
void
jerry_port_release_source (uint8_t *buffer_p) /**< buffer to free */
{
  free (buffer_p);
} /* jerry_port_release_source */

/**
 * Normalize a file path
 *
 * @return length of the path written to the output buffer
 */
size_t
jerry_port_normalize_path (const char *in_path_p, /**< input file path */
                           char *out_buf_p, /**< output buffer */
                           size_t out_buf_size, /**< size of output buffer */
                           char *base_file_p) /**< base file path */
{
  size_t ret = 0;

#if defined(_WIN32)
  size_t base_drive_dir_len;
  const size_t in_path_len = strnlen (in_path_p, _MAX_PATH);
  char *path_p;

  if (base_file_p != NULL)
  {
    char drive[_MAX_DRIVE];
    char *dir_p = (char *) malloc (_MAX_DIR);

    _splitpath_s (base_file_p, drive, _MAX_DRIVE, dir_p, _MAX_DIR, NULL, 0, NULL, 0);
    const size_t drive_len = strnlen (drive, _MAX_DRIVE);
    const size_t dir_len = strnlen (dir_p, _MAX_DIR);
    base_drive_dir_len = drive_len + dir_len;
    path_p = (char *) malloc (base_drive_dir_len + in_path_len + 1);

    memcpy (path_p, &drive, drive_len);
    memcpy (path_p + drive_len, dir_p, dir_len);

    free (dir_p);
  }
  else
  {
    base_drive_dir_len = 0;
    path_p = (char *) malloc (in_path_len + 1);
  }

  memcpy (path_p + base_drive_dir_len, in_path_p, in_path_len + 1);

  char *norm_p = _fullpath (out_buf_p, path_p, out_buf_size);
  free (path_p);

  if (norm_p != NULL)
  {
    ret = strnlen (norm_p, out_buf_size);
  }
#elif defined(__unix__) || defined(__APPLE__)
  char *base_dir_p = dirname (base_file_p);
  const size_t base_dir_len = strnlen (base_dir_p, PATH_MAX);
  const size_t in_path_len = strnlen (in_path_p, PATH_MAX);
  char *path_p = (char *) malloc (base_dir_len + 1 + in_path_len + 1);

  memcpy (path_p, base_dir_p, base_dir_len);
  memcpy (path_p + base_dir_len, "/", 1);
  memcpy (path_p + base_dir_len + 1, in_path_p, in_path_len + 1);

  char *norm_p = realpath (path_p, NULL);
  free (path_p);

  if (norm_p != NULL)
  {
    const size_t norm_len = strnlen (norm_p, out_buf_size);
    if (norm_len < out_buf_size)
    {
      memcpy (out_buf_p, norm_p, norm_len + 1);
      ret = norm_len;
    }

    free (norm_p);
  }
#else
  (void) base_file_p; /* unused */

  /* Do nothing, just copy the input. */
  const size_t in_path_len = strnlen (in_path_p, out_buf_size);
  if (in_path_len < out_buf_size)
  {
    memcpy (out_buf_p, in_path_p, in_path_len + 1);
    ret = in_path_len;
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
} /* jerry_port_get_native_module */
