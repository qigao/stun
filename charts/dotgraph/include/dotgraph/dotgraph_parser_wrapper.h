#pragma once

#include "dotgraph_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { DOTGRAPH_MAX_INPUT_BYTES = 1024 * 1024 };

/**
 * Parse a DOT graph string into a DotGraphDiagram.
 * @param input  NUL-terminated DOT source string, at most
 *               DOTGRAPH_MAX_INPUT_BYTES bytes
 * @return Diagram pointer (caller must free), or NULL on failure
 */
DotGraphDiagram* dotgraph_parse(const char* input);

/**
 * Get the current thread's last parse error message (empty string if none).
 * The returned pointer remains valid until the next parse on this thread.
 */
const char* dotgraph_get_last_error(void);

/**
 * Free a DotGraphDiagram and all owned memory.
 */
void dotgraph_diagram_free(DotGraphDiagram* diagram);

/**
 * Set measured layout size for a node (used before layout pass).
 */
void dotgraph_set_node_size(DotGraphDiagram* diagram, const char* id, double width, double height);

#ifdef __cplusplus
}
#endif
