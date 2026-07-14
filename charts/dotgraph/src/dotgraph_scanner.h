#pragma once

#include "dotgraph/dotgraph_ast.h"

typedef struct DotGraphScanner {
    const char* cursor;
    const char* limit;
    const char* marker;
    int line;
} DotGraphScanner;

void dotgraph_scan(DotGraphScanner* scanner,
                   void* parser,
                   DotGraphParserContext* context);
