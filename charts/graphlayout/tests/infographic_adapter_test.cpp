// Standalone, headless integration fixture: infographic placement consumes
// Stun::GraphLayout and must not drag in Cola or UI/rendering backends.
#include <ir/unified_infographic.h>
#include <layout/layout_engine.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace flex::modules::infographic;

namespace {
void check(bool passed, const char* what) {
    if (!passed) throw std::runtime_error(what);
}
void check_nonoverlap(const LayoutResult& layout) {
    for (std::size_t i = 0; i < layout.nodes.size(); ++i) {
        const auto& a = layout.nodes[i].bounds;
        check(std::isfinite(a.x) && std::isfinite(a.y) &&
              std::isfinite(a.width) && std::isfinite(a.height) &&
              a.width > 0 && a.height > 0,
              "finite positive infographic geometry");
        check(a.x >= 0 && a.y >= 0, "nonnegative infographic canvas placement");
        check(a.x + a.width <= layout.canvas_width &&
              a.y + a.height <= layout.canvas_height,
              "infographic canvas must include all node bounds");
        for (std::size_t j = i + 1; j < layout.nodes.size(); ++j) {
            const auto& b = layout.nodes[j].bounds;
            check(a.x + a.width <= b.x || b.x + b.width <= a.x ||
                  a.y + a.height <= b.y || b.y + b.height <= a.y,
                  "infographic cards overlap");
        }
    }
}
std::size_t find(const LayoutResult& layout, const std::string& label) {
    for (std::size_t i = 0; i < layout.nodes.size(); ++i)
        if (layout.nodes[i].item && layout.nodes[i].item->label == label)
            return i;
    throw std::runtime_error("missing infographic node: " + label);
}
}

int main() {
    try {
        {
            UnifiedInfographic info;
            for (int i = 0; i < 12; ++i)
                info.items.push_back(DataItem::create("card_" + std::to_string(i)));
            GridLayoutEngine engine;
            StyleConfig style;
            style.card_width = 115;
            style.card_height = 65;
            style.item_spacing = 12;
            style.max_columns = 3;
            const auto result = engine.compute(info, 280, 900, style);
            check(result.nodes.size() == 12, "all infographic items represented");
            check_nonoverlap(result);
        }
        {
            UnifiedInfographic info;
            auto root = DataItem::create("root");
            auto left = DataItem::create("left");
            left->children.push_back(DataItem::create("grandchild"));
            root->children.push_back(std::move(left));
            root->children.push_back(DataItem::create("right"));
            info.items.push_back(std::move(root));
            TreeLayoutEngine engine;
            StyleConfig style;
            style.card_width = 100;
            style.card_height = 45;
            style.item_spacing = 20;
            const auto result = engine.compute(info, 780, 650, style);
            check(result.nodes.size() == 4, "deep tree retains all descendants");
            check(result.parent_index.size() == 4, "parent map complete");
            check(result.parent_index[find(result,"root")] == -1, "tree root has no parent");
            check(result.parent_index[find(result,"left")] ==
                  static_cast<int>(find(result,"root")), "left parent correct");
            check(result.parent_index[find(result,"right")] ==
                  static_cast<int>(find(result,"root")), "right parent correct");
            check(result.parent_index[find(result,"grandchild")] ==
                  static_cast<int>(find(result,"left")), "deep parent remapped across layer ordering");
            check(result.nodes[find(result,"grandchild")].bounds.y >
                  result.nodes[find(result,"left")].bounds.y, "deep descendant lower rank");
            check_nonoverlap(result);
        }
        std::cout << "infographic projection adapter: all checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "infographic projection adapter: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
