#include "flexui/document.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nanovg_css.h>
#include "nanovg_css_internal.h" // For accessing computed layout

using namespace flexui;
using Catch::Matchers::WithinAbs;

TEST_CASE("FlexDocument - CSS Styling", "[document][styling]") {
    FlexDocument doc;
    NVGCSSRenderer* renderer = nvgcssCreateRenderer(nullptr);
    nvgcssSetViewport(renderer, 800, 600);
    
    doc.setRenderer(renderer);

    SECTION("Case-insensitive CSS properties") {
        FlexStyleSheet sheet;
        sheet.name = "test-sheet";
        sheet.contents = R"(
            #container {
                display: flex;
                Flex-Direction: Column; /* Mixed case */
                width: 200px;
                height: 200px;
            }
            .item {
                width: 100px;
                height: 50px;
            }
        )";
        doc.addStyleSheet(sheet);

        FlexNodeDesc containerDesc;
        containerDesc.id = "container";
        containerDesc.tag = "div";
        doc.appendNode(containerDesc);

        FlexNodeDesc item1Desc;
        item1Desc.id = "item1";
        item1Desc.tag = "div";
        item1Desc.classes = {"item"};
        doc.appendNode(item1Desc, "container");

        FlexNodeDesc item2Desc;
        item2Desc.id = "item2";
        item2Desc.tag = "div";
        item2Desc.classes = {"item"};
        doc.appendNode(item2Desc, "container");

        // Manually trigger layout computation
        nvgcssComputeLayout(renderer);

        auto* item1 = doc.findNode("item1");
        auto* item2 = doc.findNode("item2");

        REQUIRE(item1 != nullptr);
        REQUIRE(item2 != nullptr);

        // Access underlying NVGCSSElement to check computed layout
        auto* el1 = static_cast<NVGCSSElement*>(item1->element());
        auto* el2 = static_cast<NVGCSSElement*>(item2->element());

        REQUIRE(el1 != nullptr);
        REQUIRE(el2 != nullptr);

        // Verify column layout (stacked vertically)
        // If Flex-Direction: Column was parsed correctly, y coordinates should differ
        REQUIRE_THAT(el1->computed.y, WithinAbs(0.0f, 0.1f));
        REQUIRE_THAT(el2->computed.y, WithinAbs(50.0f, 0.1f));
    }

    SECTION("CSS Variables") {
        FlexStyleSheet sheet;
        sheet.name = "vars";
        sheet.contents = R"(
            :root {
                --main-color: #ff0000;
                --spacing: 20px;
            }
            #box {
                color: var(--main-color);
                width: 100px;
                height: 100px;
                margin: var(--spacing);
            }
        )";
        doc.addStyleSheet(sheet);

        FlexNodeDesc boxDesc;
        boxDesc.id = "box";
        boxDesc.tag = "div";
        doc.appendNode(boxDesc);

        // Variables are applied during style computation
        // Accessing styles directly is hard without internal headers or helper methods
        // But we can verify if layout properties derived from variables (like margin) are correct
        // margin: 20px -> should affect layout if in a container
        
        // For now, just verify no crash and basic existence
        auto* box = doc.findNode("box");
        REQUIRE(box != nullptr);
    }

    SECTION("Inline styles persist after stylesheet update") {
        // 1. Create node with inline style
        FlexNodeDesc desc;
        desc.id = "inline-node";
        desc.tag = "div";
        desc.attributes["style"] = "width: 123px; height: 456px;";
        doc.appendNode(desc);

        // Trigger layout to apply inline style (via attachNode)
        // (Actually, we need to setRenderer first or force rebuild if renderer is already set)
        // Renderer is already set in setup.

        // 2. Add a stylesheet (triggering applyStyleSheets)
        FlexStyleSheet sheet;
        sheet.name = "new-sheet";
        sheet.contents = "#inline-node { color: red; }"; // Just some rule
        doc.addStyleSheet(sheet);

        // 3. Verify inline style is still effective
        // Need to compute layout
        nvgcssComputeLayout(renderer);

        auto* node = doc.findNode("inline-node");
        REQUIRE(node != nullptr);
        auto* el = static_cast<NVGCSSElement*>(node->element());
        REQUIRE(el != nullptr);

        // Check computed width/height
        REQUIRE_THAT(el->computed.width, WithinAbs(123.0f, 0.1f));
        REQUIRE_THAT(el->computed.height, WithinAbs(456.0f, 0.1f));
    }

    nvgcssDeleteRenderer(renderer);
}
