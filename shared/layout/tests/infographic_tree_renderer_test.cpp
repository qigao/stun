#include <layout_renderer.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace flex::modules::infographic;

namespace {
void require(bool yes, const char* why) {
    if (!yes) throw std::runtime_error(why);
}
}

int main() {
    try {
        UnifiedInfographic info;
        auto root=DataItem::create("root");
        auto child=DataItem::create("child");
        auto grand=DataItem::create("grand");
        grand->children.push_back(DataItem::create("great"));
        child->children.push_back(std::move(grand));
        root->children.push_back(std::move(child));
        root->children.push_back(DataItem::create("sibling"));
        info.items.push_back(std::move(root));
        const auto* r=info.items[0].get();
        const auto* c=r->children[0].get();
        const auto* g=c->children[0].get();
        const auto* gg=g->children[0].get();
        const auto* s=r->children[1].get();
        LayoutResult layout;
        std::vector<const DataItem*> items{r,c,g,gg,s};
        for(std::size_t i=0;i<items.size();++i) {
            LayoutNode node;
            node.item=items[i];
            node.index=static_cast<int>(i);
            node.bounds={static_cast<float>(i*15),static_cast<float>(i*80),100,40};
            layout.nodes.push_back(node);
        }
        layout.parent_index={-1,0,1,2,0};
        flex::ArenaAllocator arena;
        flex::Group canvas;
        LayoutRenderContext ctx{arena,&canvas,780,650,nullptr,&layout};
        TreeLayoutRenderer renderer;
        renderer.render(info,ctx);
        std::size_t routes=0,cards=0,texts=0;
        for(const auto& el:canvas.children) {
            if(auto* shape=dynamic_cast<flex::Shape*>(el.get())) {
                if(shape->rectangle)++cards;
                else if(!shape->path.empty())++routes;
            }
            if(dynamic_cast<flex::Text*>(el.get()))++texts;
        }
        require(routes==4 && cards==5 && texts==5,
                "all deep descendants and edges must be rendered");

        canvas.children.clear();
        ctx.layout=nullptr;
        bool rejected=false;
        try { renderer.render(info,ctx); }
        catch(const std::invalid_argument&) {rejected=true;}
        require(rejected,"missing layout must not revert to shallow rendering");
        ctx.layout=&layout;
        layout.nodes.pop_back();
        rejected=false;
        try { renderer.render(info,ctx); }
        catch(const std::invalid_argument&) {rejected=true;}
        require(rejected,"missing descendant must fail");
        require(canvas.children.empty(),"missing descendant cannot draw partial graph");
        std::cout << "infographic tree renderer: all checks passed\n";
        return EXIT_SUCCESS;
    } catch(const std::exception& e) {
        std::cerr << "infographic tree renderer: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
