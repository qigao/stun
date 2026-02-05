#pragma once

#include "md_extension.h"

namespace md_re2c {

// 注册所有可用的图表处理器
// 调用一次即可，重复调用无副作用
void register_diagram_handlers();

// 单独注册各模块（如果只需要部分功能）
void register_mermaid_handler();
void register_flexchart_handler();
void register_infographic_handler();

} // namespace md_re2c
