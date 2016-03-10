/* Copyright 2015-2016 Samsung Electronics Co., Ltd.
 * Copyright 2016 University of Szeged.
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

#ifndef JERRY_PORT_H
#define JERRY_PORT_H

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif /* !__cplusplus */

/** \addtogroup jerry_port Jerry engine port
 * @{
 */

/**
 * Target port functions for console output
 */
int jerry_port_logmsg (FILE *stream, const char *format, ...);
int jerry_port_errormsg (const char *format, ...);
int jerry_port_putchar (int c);

void jerry_port_abort (void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif /* !__cplusplus */
#endif /* !JERRY_PORT_H */
