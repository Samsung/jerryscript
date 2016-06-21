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

#ifndef ECMA_STRING_OBJECT_H
#define ECMA_STRING_OBJECT_H

#include "ecma-globals.h"

/** \addtogroup ecma ECMA
 * @{
 *
 * \addtogroup ecmastringobject ECMA String object related routines
 * @{
 */

extern ecma_value_t
ecma_op_create_string_object (const ecma_value_t *, ecma_length_t);

extern ecma_property_t *
ecma_op_string_object_get_own_property (const ecma_object_t *, ecma_string_t *);

extern void
ecma_op_string_list_lazy_property_names (ecma_object_t *,
                                         bool,
                                         ecma_collection_header_t *,
                                         ecma_collection_header_t *);


/**
 * @}
 * @}
 */

#endif /* !ECMA_STRING_OBJECT_H */
