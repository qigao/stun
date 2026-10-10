#pragma once
#include <cmath>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <utility>
namespace flex {
struct Color { float r,g,b; Color(float a,float bb,float c):r(a),g(bb),b(c){} };
enum class Anchor { Center };
struct Drawable { virtual ~Drawable()=default; };
struct Shape:Drawable {
    std::string path;
    bool rectangle=false;
    void set_path(const std::string &p){path=p;}
    void set_stroke(Color,float){}
    void set_rect(float,float,float){rectangle=true;}
    void set_fill(Color){}
    void set_position(float,float){}
};
struct Text:Drawable {
    std::string content;
    void set_content(const std::string &s){content=s;}
    void set_font_size(float){}
    void set_color(Color){}
    void set_position(float,float){}
    void set_anchor(Anchor){}
};
struct Group {
    std::vector<std::shared_ptr<Drawable>> children;
    void add_child(std::shared_ptr<Drawable> p){children.push_back(std::move(p));}
};
struct ArenaAllocator {
    template <typename T> std::shared_ptr<T> create(){return std::make_shared<T>();}
};
}
namespace flex::modules::infographic {
struct DataItem {
    std::string label;
    std::vector<std::unique_ptr<DataItem>> children;
    static std::unique_ptr<DataItem> create(std::string s) {
        auto ptr=std::make_unique<DataItem>();ptr->label=std::move(s);return ptr;
    }
};
struct UnifiedInfographic {
    std::vector<std::unique_ptr<DataItem>> items;
    struct Theme{} theme;
};
struct StyleConfig {int card_width=120,card_height=60,item_spacing=20;};
struct LayoutRect{float x=0,y=0,width=0,height=0;};
struct LayoutNode{
    LayoutRect bounds;
    const DataItem* item=nullptr;
    int index=0;
    std::vector<LayoutNode> children;
};
struct LayoutResult {
    int canvas_width=800,canvas_height=600,content_start_y=100;
    std::vector<LayoutNode> nodes;
    std::vector<int> parent_index;
};
class TreeLayoutEngine {
public:
 LayoutResult compute(const UnifiedInfographic&,int,int,const StyleConfig&);
};
struct LayoutRenderContext {
    flex::ArenaAllocator& arena;
    flex::Group* root;
    float width=0,height=0;
    void* instance=nullptr;
    const LayoutResult* layout=nullptr;
};
class TreeLayoutRenderer{
public: void render(const UnifiedInfographic&,LayoutRenderContext&);
};
}
