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

void flowchart_set_node_size(FlowchartDiagram* diagram, const char* id, double width, double height);

void flowchart_set_layout_mode(FlowchartDiagram* diagram, int mode);

void flowchart_set_routing_mode(FlowchartDiagram* diagram, int mode);

#ifdef __cplusplus
}
#endif
