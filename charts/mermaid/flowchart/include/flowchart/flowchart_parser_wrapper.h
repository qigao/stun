#pragma once

#include "flowchart_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FlowchartParseStatus {
    FLOWCHART_PARSE_ERROR = -1,
    FLOWCHART_PARSE_COMPLETE = 0,
    FLOWCHART_PARSE_PARTIAL = 1
} FlowchartParseStatus;

/**
 * 严格解析 Mermaid 流程图代码，并显式报告是否产生了部分 AST。
 * @param input 源码字符串，不可为 NULL
 * @param out_diagram 接收 AST；COMPLETE/PARTIAL 时由调用方释放
 * @return COMPLETE 表示无语法错误，PARTIAL 表示保留了错误前的 AST，ERROR 表示无可用 AST
 */
FlowchartParseStatus flowchart_parse_ex(const char* input, FlowchartDiagram** out_diagram);

/**
 * 兼容解析入口。语法错误发生前已有内容时返回部分 AST。
 * 新代码应使用 flowchart_parse_ex() 区分完整与部分结果。
 * @param input 源码字符串
 * @return 无可用 AST 返回 NULL，否则返回需要手动释放的 FlowchartDiagram 指针
 */
FlowchartDiagram* flowchart_parse(const char* input);

/**
 * 获取最近一次解析错误（若无错误则为空字符串）
 */
const char* flowchart_get_last_error();

/**
 * 释放 FlowchartDiagram 占用的内存
 */
void flowchart_diagram_free(FlowchartDiagram* diagram);

/** Borrowed metadata, valid until the owning parsed diagram is freed.
 * Pass only edges returned by flowchart_parse[_ex], not caller-built edges.
 * NULL means that no explicit ID / curve was declared. */
const char* flowchart_edge_get_id(const FlowchartEdge* edge);
const char* flowchart_edge_get_curve(const FlowchartEdge* edge);

typedef enum FlowchartJsonFormat {
    FLOWCHART_JSON_LEGACY = 1,
    FLOWCHART_JSON_SOURCE_V2 = 2
} FlowchartJsonFormat;

/** Explicit output contract. SOURCE_V2 requires parser-owned diagrams/edges,
 * emits links in AST declaration order, and exports arrow/ID/curve metadata.
 * Unsupported formats/operators return NULL; there is
 * no fallback to another schema. Allocation/insertion failure frees the entire
 * temporary DOM and returns NULL rather than partial JSON. Free successful
 * output with json_serialize_free.
 * The returned buffer is independent of the diagram's lifetime. */
char* flowchart_to_json_ex(const FlowchartDiagram* diagram, FlowchartJsonFormat format);
/** Existing sparse projection; equivalent to explicitly selecting LEGACY. */
char* flowchart_to_json(FlowchartDiagram* diagram);

void flowchart_set_node_size(FlowchartDiagram* diagram, const char* id, double width, double height);

void flowchart_set_layout_mode(FlowchartDiagram* diagram, int mode);

void flowchart_set_routing_mode(FlowchartDiagram* diagram, int mode);

#ifdef __cplusplus
}
#endif
