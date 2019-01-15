#!/usr/bin/env python

# Copyright JS Foundation and other contributors, http://js.foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from collections import namedtuple
import os
import sys

# This script is normally invoked directly by test-heap-snapshot.c.
# We read the provided heap snapshot and verify various features,
# an easier feat in python than in C.

if len(sys.argv) != 2:
  sys.stderr.write("%s unit-test-heap-snapshot.tsv" % sys.argv[0])
  sys.exit(0)

Node = namedtuple("Node", "id type size representation representation_node")
Edge = namedtuple("Edge", "parent node type name name_node")
NULL_REFERENCE = "0"

nodes = {}
edges = {}

for line in open(sys.argv[1], "r"):
  if not line.strip():
    continue
  components = line.strip().split("\t")
  if components[0] == "NODE":
    node = Node(*components[1:])
    nodes[node.id] = node
  elif components[0] == "EDGE":
    edge = Edge(*components[1:])
    edges[(edge.parent, edge.node)] = edge
  else:
    raise RuntimeError("Invalid line in heap snapshot: %s" % line)

def assert_lineage(*lineage):
  """ Checks that there exists a sequence of nodes joined by edges
      where some subset of the sequence has representations or edge (names)
      matching the provided lineage, ordered by generation ascending.
  """
  class LineageSatisfied(Exception):
    pass

  visited_elements = set()
  def dfs(element, remaining_lineage):
    if element in visited_elements:
      return
    visited_elements.add(element)
    next_label = remaining_lineage[0]
    if next_label.startswith("(") and type(element) is Edge and element.name == next_label[1:-1]:
      remaining_lineage = remaining_lineage[1:]
    elif not next_label.startswith("(") and type(element) is Node and element.representation == next_label:
      remaining_lineage = remaining_lineage[1:]
    if not remaining_lineage:
      raise LineageSatisfied()

    if type(element) is Edge:
      dfs(nodes[element.node], remaining_lineage)
    else:
      for e in edges.values():
        if e.parent == element.id:
          dfs(e, remaining_lineage)

  # Find first element to start DFS from.
  first_label = lineage[0]
  if first_label.startswith("("):
    first_element = next(iter(e for e in edges.values() if e.name == first_label[1:-1]))
  else:
    first_element = next(iter(n for n in nodes.values() if n.representation == first_label))

  try:
    dfs(first_element, lineage[1:])
  except LineageSatisfied:
    return
  else:
    raise RuntimeError("Lineage %s not satisfied" % (lineage,))

# At this point the heap snapshot is loaded, and we can inspect it for correctness.
try:
  # Verify that all node references are resolved.
  for node in nodes.values():
    if node.representation_node != NULL_REFERENCE:
      assert node.representation_node in nodes, \
        "Node %s references representation %s not found in heap snapshot" % (node, node.representation_node)

  for edge in edges.values():
    if edge.name_node != NULL_REFERENCE:
      assert edge.name_node in nodes, \
        "Edge %s references name %s not found in heap snapshot" % (edge, edge.name_node)
    assert edge.parent in nodes, \
      "Edge %s references parent %s not found in heap snapshot" % (edge, edge.parent)
    assert edge.node in nodes, \
      "Edge %s references destination %s not found in heap snapshot" % (edge, edge.node)

  # Confirm that we are seeing some structures we expect given the sample JS code.
  # These paths are specified as node representations & (edge names), in descending order.
  assert_lineage("(user_object)", "(instantiated_object)", "complex_attribute")
  assert_lineage("(user_object)", "(instantiated_object)", "(complex_attribute)", "key1")
  assert_lineage("(user_object)", "(instantiated_object)", "(complex_attribute)", "(key1)", "value1")
  assert_lineage("(user_object)", "(instantiated_object)", "(complex_attribute)", "key2")
  assert_lineage("(user_object)", "(instantiated_object)", "(complex_attribute)", "(key2)", "longer value2")
  assert_lineage("(user_object)", "(instantiated_object)", "recycled_attribute")
  assert_lineage("(user_object)", "(instantiated_object)", "(recycled_attribute)", "Date")
  assert_lineage("(user_object)", "(instantiated_object)", "simple_attribute")
  # NB. No heap-allocated value for simple_attribute - it's packed into the prop header.

except:
  raise
else:
  # No failures, delete tempfile.
  os.unlink(sys.argv[1])
