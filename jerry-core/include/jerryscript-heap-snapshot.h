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

#ifndef JERRY_HEAP_SNAPSHOT_H
#define JERRY_HEAP_SNAPSHOT_H

#include "jerryscript-core.h"

typedef enum
{
  JERRY_HEAP_SNAPSHOT_NODE_TYPE_HIDDEN,
  JERRY_HEAP_SNAPSHOT_NODE_TYPE_ARRAY,
  JERRY_HEAP_SNAPSHOT_NODE_TYPE_STRING,
  JERRY_HEAP_SNAPSHOT_NODE_TYPE_OBJECT,
  JERRY_HEAP_SNAPSHOT_NODE_TYPE_CODE,
  JERRY_HEAP_SNAPSHOT_NODE_TYPE_CLOSURE,
  JERRY_HEAP_SNAPSHOT_NODE_TYPE_NATIVE,
  JERRY_HEAP_SNAPSHOT_NODE_TYPE__COUNT = JERRY_HEAP_SNAPSHOT_NODE_TYPE_NATIVE
} jerry_heap_snapshot_node_type_t;

typedef enum
{
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_HIDDEN,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_LEXENV,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROTOTYPE,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_BIND,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_THIS,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_BIND_ARGS,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_ELEMENTS,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROPERTY,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROPERTY_NAME,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROPERTY_GET,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROPERTY_SET,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROMISE_RESULT,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROMISE_FULFILL,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_PROMISE_REJECT,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_MAP_ELEMENT,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE_SCOPE,
  JERRY_HEAP_SNAPSHOT_EDGE_TYPE__COUNT = JERRY_HEAP_SNAPSHOT_EDGE_TYPE_SCOPE
} jerry_heap_snapshot_edge_type_t;

typedef uintptr_t jerry_heap_snapshot_node_id_t;

typedef void (*jerry_heap_snapshot_node_callback_t)(jerry_heap_snapshot_node_id_t node,
                                                    jerry_heap_snapshot_node_type_t type,
                                                    size_t size,
                                                    jerry_value_t representation,
                                                    jerry_heap_snapshot_node_id_t representation_node,
                                                    void *user_data_p
                                                    );

typedef void (*jerry_heap_snapshot_edge_callback_t)(jerry_heap_snapshot_node_id_t parent,
                                                    jerry_heap_snapshot_node_id_t node,
                                                    jerry_heap_snapshot_edge_type_t type,
                                                    jerry_value_t name,
                                                    jerry_heap_snapshot_node_id_t name_node,
                                                    void *user_data_p
                                                    );

void jerry_heap_snapshot_capture (jerry_heap_snapshot_node_callback_t node_cb,
                                  jerry_heap_snapshot_edge_callback_t edge_cb,
                                  void *user_data_p);

#endif
