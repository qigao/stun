#pragma once

#include "flowchart_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 解析 Mermaid 流程图代码
 * @param input 源码字符串
 * @return 失败返回 NULL，成功返回需要手动释放的 FlowchartDiagram 指针
 */
FlowchartDiagram* flowchart_parse(const char* input);

/**
 * 释放 FlowchartDiagram 占用的内存
 */
void flowchart_diagram_free(FlowchartDiagram* diagram);

void flowchart_set_node_size(FlowchartDiagram* diagram, const char* id, double width, double height);

void flowchart_set_layout_mode(FlowchartDiagram* diagram, int mode);

#ifdef __cplusplus
}
#endif
