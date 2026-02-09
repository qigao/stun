#include "md_adapters.h"

// 条件编译：只有链接了对应模块才启用
#ifdef MD_HAS_FLEXMAID
#include <flexmaid/flexmaid.h>
#endif

#ifdef MD_HAS_FLEXCHART
#include <flexchart/flexchart.h>
#endif

#ifdef MD_HAS_INFOGRAPHIC
#include <infographic/flexinfographic.h>
#endif

namespace md {

void register_mermaid_handler() {
#ifdef MD_HAS_FLEXMAID
    ExtensionRegistry::instance().register_handler("mermaid", [](std::string_view content) {
        flex::modules::flexmaid::FlexMaid maid;
        BlockResult result;
        result.type = BlockResult::Type::Svg;
        result.content = maid.mermaid_to_svg(std::string(content));
        return result;
    });
#endif
}

void register_flexchart_handler() {
#ifdef MD_HAS_FLEXCHART
    ExtensionRegistry::instance().register_handler("flexchart", [](std::string_view content) {
        BlockResult result;
        result.type = BlockResult::Type::Text;
        // flexchart 返回 AST，需要额外渲染步骤
        // 这里先返回原文，实际使用时需要配合渲染器
        result.content = std::string(content);
        return result;
    });
#endif
}

void register_infographic_handler() {
#ifdef MD_HAS_INFOGRAPHIC
    ExtensionRegistry::instance().register_handler("infographic", [](std::string_view content) {
        flex::modules::infographic::FlexInfographic info;
        BlockResult result;
        result.type = BlockResult::Type::Svg;
        result.content = info.infographic_to_svg(std::string(content));
        return result;
    });
#endif
}

void register_diagram_handlers() {
    register_mermaid_handler();
    register_flexchart_handler();
    register_infographic_handler();
}

} // namespace md
